# Original Overworld Mob constructors

This CPU-only reference package executes unchanged original client constructors
for Spider `cn`, Zombie `uz`, Skeleton `fr`, Creeper `gb`, Slime `uw`, Pig `wh`,
Sheep `dl`, Cow `bx`, Chicken `ww`, Wolf `gi` and Squid `xt`.
It runs 33 constructors in an ordinary VM and another 33 under a read-only JDI
observer. Java is reference tooling; no Java game implementation is provided.

```sh
python -B tests/reference/entity/mob_construction/capture_reference.py \
  --client-jar /private/b1.7.3.jar --output .local/mob-construction-new
```

Use matching JDK 17 `java` and `javac`, optionally selected with `--java` and
`--javac`. The output directory must be new. No original classes, resources,
assets or generated expected-output answers are distributed here. Raw output,
constructor projections, actual entropy and class-init logs stay in the chosen
ignored output directory. The wrapper pins the supplied JAR, complete class
hashes and method Code identities, tools, authored sources and compiled helpers.
It checks before/after fingerprints and independently verifies live JVM Code
bytes at every observed source method. Classpaths contain only the three
authored helpers and the verified original JAR. No device SDK is needed.

## Inputs and initialization context

The target genuinely constructs `fd(new wy(), ..., xa.a(0), 1234L)`, producing
an actual Overworld `rh` and provider. A genuine `gr("MobSetup", "")` Session
prepares the ordinary `uu`-before-`gm` registry initialization order before the
observed Mob calls. This is an explicit prepared registry context.

The original cold Skeleton-first attempt was preserved privately in capture
`a`: `fr.<clinit>` reached `gm -> uu -> jl -> ep -> ny -> iz`, with an actual
null Item receiver in `iz.<init>`, yielding ExceptionInInitializerError.
Successful prepared-context results do not establish arbitrary cold-init order.

Cases 0 through 14 retain the three hostile passes in cn, uz, fr, gb, uw order.
Cases 15 through 32 append three passive passes in wh, dl, bx, ww, gi, xt order:

| Pass | Constructor World argument | Actual World.B |
| --- | --- | --- |
| 0 | Genuine shared World | false |
| 1 | Same genuine World | true |
| 2 | null | false |

There are no field assignments to an Entity, RNG, watcher, bounds object,
Entity counter or source static registry. Constructor code and virtual children
are unchanged. The target only prepares World.B between independent calls.
Its authored `activeCase` field admits observer events; the observer watches
that field but never modifies it. The actual shared Math holder is initialized
once before the active cases, read without reseeding and allowed to continue.

The exact 190-class original initialization closure is pinned and checked in
both VMs, including the genuine registry preparation. Runtime class resources
and code-source origins are checked for every pinned class. Minecraft/PX,
LWJGL and sound bootstrap remain excluded.

## Constructor order and source boundaries

The four ordinary hostile classes follow concrete -> gz -> ii -> ls -> sn;
Slime follows uw -> ls -> sn. Base sn creates its own ordinary Random at PCs
123/127/130, creates its watcher at 183/187/190, stores its actual World at 205,
calls actual virtual position at 212, adds watcher index 0 at 224 and invokes
the concrete watcher hook at 228. Those hooks run before Living initialization.

Creeper's hook calls inherited initialization, then adds Byte -1 at index 16
and Byte 0 at index 17. Slime's hook calls the real inherited hook and adds a
new Byte 1 at index 16. Slime later updates that record through actual
`uw.e(I)V`, including its original dirty-state change.

Living consumes four actual Math.random calls at PCs 93,156,185,196. Their
stores precede checkpoints 108,168,195,211; final Living return is 217. GZ then
sets its damage field to 2 and health to 20. Spider changes texture, calls actual
size (1.4,.9), then changes movement speed. Zombie changes texture, speed and
damage. Skeleton and Creeper change texture. Skeleton's real class initializer
constructs one static `iz(gm.i,1)` Stack; all three Skeleton calls return that
same object through actual `r_()`.

Slime draws actual private Random.nextInt(3) at PC 22 for size `1 << draw`,
sets eye offset zero, draws nextInt(20) at 39 for jump delay `draw+10`, then
calls actual size helper at 50. The helper updates watcher 16, computes size
using float .6 times size, sets health size squared and updates actual bounds
through the real position call. These are constructor effects, not observer
field preparation.

The five land animals follow BG -> II -> LS; Squid follows AR -> II -> LS.
Their leaf size calls change width and height without repairing the earlier
bounds. Chicken sets health 4 and draws its egg timer through nextInt(6000)+6000.
Squid sets fourteen own floats to zero, then computes its phase speed with one
private nextFloat and the original float add, division and multiplication.

Pig and Sheep register Byte16=0. Wolf registers Byte16=0, String17="" and
Integer18 from the still-zero Living health field. The actual inherited base
hook runs before Living health 10 and Wolf health 8. Sheep's class initializer
allocates a distinct float[3] for each of 16 colors and publishes the outer
array only at putstatic348. The observer checks its null prefix and completed
publication; ordinary rows retain all colors and static/row identity checks.

`source-pins.json` includes complete SN method identities, including
`sn.c(DDDFF)V`. `operation-map.json` includes its field/call map for the later
World spawn-position continuation. That pose method is not invoked by this
constructor package.

## Observations and random evidence

The target records every nonstatic field across the concrete class and all
original Entity ancestors, all six actual bounds fields, actual watcher
membership/type/value/dirty state and actual Entity counter consumption.
Float and double values retain raw bits. Reference identities are kept for
alias checks; allocator identities are not cross-process equalities. Skeleton
records all four actual Stack fields and its real static/getter aliases.

JDI records constructor and watcher checkpoints, including the actual private
Random state immediately after its sn store, base watcher completion, all four
Math poststores, completed Living and concrete constructor state, and Slime's
two private draws and size-helper mutations, Chicken and Squid postdraw states,
Wolf's typed hook prefixes and Sheep's initializer publication. It verifies 25 live source method
bodies. It never writes a target field, invokes a target method, changes a clock,
replaces a Random or rewrites a class.

The ordinary and observed constructors have independent entropy. The validator
compares deterministic fields directly and separately checks each VM's four
actual Math draws against its own pre/post state and resulting fields. Python
independently replays the 48-bit LCG; JDI checks the intermediate original
poststore boundaries. The four resulting Living fields are aj, G, F and yaw.
No inferred draw tape enters an original constructor.

During each active constructor, read-only JDI also observes actual return values
from JDK `Random.seedUniquifier()` and native `System.nanoTime()`. Their xor,
combined with the normal Random constructor transform, must produce the actual
post-store private seed. These U/nano values are actual observations, not values
recovered from the private seed. Slime's actual nextInt outcomes, draw counts
and poststates are independently replayed from that observed seed, including
the rejection rule. Chicken's bounded egg draw and Squid's 24-bit float draw
are replayed separately; the other constructors consume no private draws.

Clock-event requests are enabled only during active Mob constructors. They do
not capture the earlier World/registry default-Random prefix, and do not license
rebinding a native profile after its owners exist. A native fixture may apply
its established fresh-profile admission or separately labelled scalar replay
rules. Matching two independent native constructors is not original parity.

## Private transport and finite scope

Each run writes:

* `normal/observations.ndjson`: setup plus 33 ordinary original constructor rows.
* `debugger/target-observations.ndjson`: setup plus 33 independently observed rows.
* `debugger/checkpoints.ndjson`: live Code pins, actual checkpoints and U/nano
  return records, in observed order.
* `observations.json`: `normal_rows`, `debugger_rows`, `entropy_profiles`,
  `checkpoints` and `clock_rows`, for a native comparator.
* `capture.json`: validation counts, observed Slime sizes, tool/source/helper
  fingerprints and final PASS/FAIL status.

Case rows use `fields` dictionaries keyed by actual declaring class and field
name. Primitive floats/doubles are `{"F":"8hex"}`/`{"D":"16hex"}`; other
primitive scalars are JSON values. References have actual class and an authored
identity, with String contents preserved. `bounds`, `watcher`, `private_random`
and optional `skeleton_stack` are separate records. Entropy profiles retain
actual U/nano words, before/after private seeds, nextInt outcomes/counts and
four Math draw words. They preserve actual generated values without claiming
independent constructors happened to share them.

The original 15 case IDs and nextInt profile fields remain stable. Passive case
IDs start at 15. Squid's profile adds `next_float_words` and
`next_float_draw_counts`; these are empty arrays for every other constructor.

This finite reference covers successful prepared-context client constructors
and observed prefixes. Slime size coverage depends on the actual ordinary
entropy and is reported per run, never forced or assumed. World insertion,
spawn eligibility, AI, gameplay ticks, drops, rendering, server constructors,
arbitrary cold initialization and matched gameplay output remain separate.
