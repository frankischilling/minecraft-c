# Mob active ticks

`mc_mob_tick` dispatches the actual client `w_()` chains for Spider, Zombie,
Skeleton, Creeper, Slime, Pig, Sheep, Cow, Chicken, Wolf and Squid. World Entity
updates call it at `fd.a(Entity,boolean):121`, using the application's existing
Registry and Math owners. The receiver remains a genuine Mob. Shared code reads
its embedded LivingEntity fields; it never creates or casts a Player to stand
in for those fields.

## Source bodies and order

The authority is the unchanged Beta 1.7.3 client JAR, SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The [original harness](../../tests/reference/entity/mob_tick/README.md) records
the relevant class and live method pins, authored operands and independent
constructor observations.

The concrete wrappers preserve their inherited calls. Living `w_` enters the
virtual environment update before virtual `o`, then completes the heading,
walking and angle calculations. Monster peaceful removal follows the complete
inherited tick. Creeper fuse history, Slime landing, Chicken animation and egg
timers, Wolf shake and interest state, and Squid phases retain their own order.

Living `U` stores `W=X` before the Entity body. Its ambient test draws
`nextInt(1000)`, postincrements the counter and compares the old value. The
negative reset uses the actual virtual interval: Animal and WaterCreature use
120; the Living default is 80. Literal, null and dynamic Wolf sound results
continue through the actual sound body and its two private Random floats.

Mob environment, action and travel use the actual Living fields, virtual eye
heights and watcher values. Wolf eye height is `height*.8`; the ordinary Living
formula is `height*.85`. Squid keeps its own immersion and movement branches.
Movement uses the existing material scans, collision lists, bounds, movement
effects and nearby Entity push bodies. Public helpers that admit only
LocalPlayer retain that admission; the internal Mob chain enters its matching
receiver-specific body.

Creature behavior preserves both conditional `nextInt(80)` calls. A wander
request evaluates ten candidate triples and all reached weight queries. It
retains the winning coordinates before the actual [World path invocation](../world/pathfinding.md).
Returned Paths are stored and followed through their shared cursors. Wolf's hunt
uses the World Random and the actual Sheep-class result list. Sight uses the
existing [World ray tracer](../../include/mc/world/runtime/raytrace.h), including its real pooled
vector operands and reached block queries. A failed ray is never treated as a
clear line of sight.

## Missing children

These are reached dependencies, not successful empty implementations:

| Source boundary | Pending work |
|---|---|
| `ii.f_:97/109` | Concrete attack or Creeper unseen-target behavior where selected. |
| `ww.o:217` | Actual Item drop after egg sound and Egg static-field access. |
| Living/Monster/Sheep/Wolf hurt entries | Damage and downstream combat, death or drop work. |
| Selected Wolf children | Owner following, selected-Item interest and other reached behavior still requiring its own body. |
| Selected movement children | Mounting, contact attack, Slime splitting and other unimplemented subtype effects. |

The diagnostics retain actual evaluated path coordinates or target/range,
Egg item/count, and hurt source/amount. Path results come from the complete
entered World/PathFinder body; a failed search is not a returned null Path.
Remaining drop and hurt boundaries preserve their evaluated operands.
Constructor diagnostics remain unchanged by ticking.

## Native ownership

Each synchronous attempt holds the actual Mob, Registry and Math owners through
callbacks. An active receiver rejects reentry. Native iteration ceilings stop
before the next pending iteration and preserve all earlier fields, streams,
events and pool effects. The direct concrete Random counters exclude calls made
inside shared child bodies; they are not a whole-tick entropy count.

Living look targets and Creature targets have independent native owning
occurrences. Stores retain the new occurrence before publishing it and releasing
the old one. Aliases count separately; repeated stores of the same target have
zero net hold change. Selected field reads validate their corresponding owning
cell when reached. Final generic Mob destruction drains target chains through
an allocation-free queue instead of recursive C destructor calls.

[Terminal World admission](../world/terminal-retirement.md) counts the complete
target graph, including aliases, cycles, target-only nodes and Piston's static
Entity list. It refuses external owners before mutation. Native container
retirement pins the admitted graph, terminates World source admission, then
consumes the graph and container occurrences. It does not clear fields to make
a failed tick or an externally held Mob pass admission.

Ray diagnostics flatten selected child errors by value before retirement.
Producer receipt views remain borrowed from their actual live producers; no
diagnostic points into retired ray scratch. Normal child retirement is immediate.
If checked native retirement refuses, the genuine Mob retains the pending ray
and argument occurrences with a transferred self-hold. `mc_mob_tick_cleanup`
retries native cleanup without replaying source work. Pending cleanup blocks
another tick and terminal World admission. The ordinary idle/reentry cleanup
paths are tested; forced partial-retirement failure and retry remain unverified.

Path calls use the same checked-cleanup rule. A successful source assignment
owns the returned Path before the World call retires. Later cleanup failure
does not repeat that assignment or search. Idle cleanup preserves a held Path's
array, Points, cursor, holds and arena. Pending path cleanup blocks reentry and
terminal admission independently of the source Path field.

## Reference controls

The original package contains 62 authored scenarios across all eleven kinds.
Native comparison now uses 52 complete final records, two callback failures,
four source-null failures and four remaining drop/hurt prefixes. Four path
scenarios use their retained original full final records instead of the earlier
entry-prefix observations. Prefix observations include
typed inherited and concrete fields, bounds, watchers, actual streams, pool
cursors, secondary Entities, reference roles, arguments, source frames and events
already emitted before that child. Later source effects are not copied into a
native prefix.

Original capture verifies 201 classes, 200 initialized classes and 247 live Code
bodies. It records 94,943 method entries, 94,914 returns and 779 Random returns.
Its 67 constructor contexts and 69 observed default-Random occurrences retain
their raw entropy. Fixed mutable tick operands and caller `setSeed` inputs are
declared before the observed call. Independent constructor clocks, Math profiles
and namespace-local absolute Entity IDs are not equated.

The public native test uses real Worlds with 25 stored Chunks, genuine
constructors, ordinary Entity publication and production queries. Native-only
controls cover callback holds/reentry, pending iteration limits, target aliases
and cycles, Piston-only roots, terminal refusal/retry, and 16,384-node raw target
chains through generic release and aliased replacement. Target and Piston
cycles also share a genuine returned Path. A two-Cow World loop verifies a
shared cursor, source completion/clear, the other owner's reached array-index
failure and cleanup without source repair. Piston push parity remains separate;
direct Path behavior has its own original comparison.

Build `mc_test_mob_tick`, then run `ctest -R '^entity.mob_tick$' --output-on-failure`
from the configured build directory, adding `-C Debug` for the Windows
multi-configuration build. The optional original comparison is separate:

```powershell
python -B tests/tools/entity/test_mob_tick.py --executable <native-executable> --work-dir .local/mob-native-comparison --observations <fresh-original-capture>/observations.json
```

The comparison checks the current original inputs and compiled helpers before
accepting its observations. Public testing needs no original JAR, assets or JVM.
Source/null/callback failures, missing-child prefixes and native policy controls
remain separate outcomes. This module does not establish complete Mob behavior,
Entity drawing, the full World loop or a playable client.

## Focused native qualification

Windows MSVC Debug and Linux Clang18 ASan/UBSan each pass 113,936 Mob checks
over 62 scenarios and thirteen native controls. Original comparisons verify
19,832 typed field/state checks, 14,558 cross-phase reference relations and all
72 events. Six deliberately wrong reference mutations are rejected. The four
new full path scenarios retain their evaluated operands and sight-ray argument
words. Constructor entropy remains independent and unnormalized.

The paired runs use ready Registry classes. Cold initializer classification and
exceptional pending-child partial-retirement failure/retry remain unqualified.
The default loaded client uses its actual application profile; no experiment
seed or constructor replay is installed there. Broader integration checks are
recorded in [test scope](../testing.md).
