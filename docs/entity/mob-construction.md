# Client Mob construction

The client implements the actual Spider (`cn`), Zombie (`uz`), Skeleton (`fr`),
Creeper (`gb`), Slime (`uw`), Pig (`wh`), Sheep (`dl`), Cow (`bx`), Chicken (`ww`),
Wolf (`gi`) and Squid (`xt`) constructors. These cover the eleven default Mob
kinds. The public contract is
[`mc/entity/mob/construction.h`](../../include/mc/entity/mob/construction.h).
Nether Mob constructors, Mob paths, combat and drawing remain unfinished.

Each Mob has its own native allocation containing the real Entity and Living
fields. It shares the existing application's Entity classes and Random profile;
it does not borrow a Player's storage or copy a World Random. The existing local
Player constructor and these Mob constructors use one shared Living constructor
body, preserving the four actual `Math.random()` calls at LS93, 156, 185 and 196.
The base constructor still creates the separate instance Random first.

## Constructor order

The Entity constructor invokes the selected virtual watcher hook before any
Living field initialization or Math draw. Creeper registers Byte16=-1 and
Byte17=0. Slime registers Byte16=1. Spider, Zombie and Skeleton use the inherited
empty hook. Pig and Sheep register Byte16=0. Wolf registers Byte16=0, an empty
String at 17 and an Integer at 18 read from the still-zero Living health field.
The real watcher record identities, types, values and dirty flags remain inspectable.

Spider, Zombie, Skeleton and Creeper then execute Living, Creature and Monster
construction before their own fields. Monster sets damage to 2 and health to
20; Zombie changes damage to 5 and speed to 0.5. Spider sets width 1.4, height
0.9 and speed 0.8. A constructor's size stores alone do not recompute its bounds.

Slime extends Living directly. Its private Random selects a size of 1, 2 or 4
and a jump delay from 10 through 29, in source order. Its size helper updates the
actual watcher only when the Byte value differs, sets both dimensions to
`0.6f * size`, stores health as `size * size`, and positions the same bounds.

Skeleton initializes its static bow Stack before allocating an Entity header.
The superclass Entity counter initializer runs first. The Skeleton initializer
allocates a real RAW Stack, reads the actual Item `GM.i`, constructs the Stack,
then publishes `FR.a`. Registry preparation follows the caller's actual history;
the factory does not initialize Blocks to repair a cold Item failure. The static
Stack or failed initializer prefix belongs to Entity classes until their final
retirement, without retaining a Registry cycle.

Sheep initializes its real static color array before Entity allocation. The
outer array and sixteen independently allocated float[3] rows remain owned by
Entity classes, including any failed initializer prefix. The static field is
published only after all rows are complete. Chicken draws its egg delay from
its own Random; Squid draws its rotation increment there. Neither draw comes
from World Random or the shared Math stream.

## Ownership and reached failures

Allocation publishes one RAW occurrence after selected class initialization.
Construction is one-shot: it enters CONSTRUCTING and ends READY only at the real
leaf return. A reached failure retains FAILED_CONSTRUCTION, including completed
IDs, fields, entropy and child allocations. It cannot be retried. Skeleton
initializer failure latches separately from instance construction failure.

`mc_mob_retire` consumes a final inactive occurrence and refuses external holds.
Ordinary Entity retain/release also apply. Native allocation limits and platform
failures remain typed native policies; they are not invented reflection
exceptions. Source exceptions and the first initializer cause have separate
diagnostics.

The [World scheduler](../world/ticking.md) retains its selected candidate through
failure. After an admitted constructor returns, it reads the fresh World Random
at CQ808, multiplies by 360 as a float, and executes the real inherited position
and rotation body at CQ815. The [virtual predicate](mob-spawning.md) runs at
CQ820. False results continue the actual attempt loop; true results enter
publication and postspawn work. Retiring the scheduler drops its candidate
occurrence; an explicitly retained candidate can outlive that call.

## Evidence and limits

The [original constructor package](../../tests/reference/entity/mob_construction/README.md)
uses unchanged original game classes with an ordinary VM and a read-only JDI
observer. Captured entropy and scalar formula replay are distinguished from
actual native constructor execution. The native contracts separately exercise
shared profiles, watcher order, partial allocation ownership and scheduler
continuation. Their exact host results are recorded in [testing](../testing.md).

These finite checks do not establish a complete World tick, original Entity
drawing or playable gameplay.

## Qualified default-Mob continuation

All 68 selected targets build on Windows MSVC Debug and Linux Clang18 ASan/UBSan.
All 77 Windows tests pass in 105.78 seconds and all 73 Linux tests in
141.36 seconds, without failures, skips or sanitizer findings. All 2,387 C,
helper, build and reference inputs remain unchanged through both builds and tests.
Formatting checks 1,665 files; source bindings cover 7,766 entries and 3,324 files.

Each host passes 1,667 constructor checks over 33 actual constructors, with 8,244
independent original field comparisons and 165 separately labelled scalar checks.
Spawning adds 6,451 checks over 115 genuine predicates and 18 Chunk Random leaves;
136 selected predicate/Chunk/thunder rows match the original exactly. Original
constructor capture checks 25 live methods and 561 checkpoints. Predicate capture
checks 19 live methods and 2,138 checkpoints. Independent JVM/native constructor
entropy is not equated.

## Preceding hostile-constructor checkpoint

The following results qualify published source `71aedbd` before the passive
constructors and spawn predicates were added. Its four hosted CI jobs pass.

All 67 selected targets build on Windows MSVC Debug and Linux Clang18 ASan/UBSan. All 76
Windows tests pass in 89.99 seconds and all 72 Linux tests in 118.22 seconds, without
failures, skips or sanitizer findings. All 2,365 source, helper, build and reference
inputs remain unchanged through both builds and tests; formatting checks 1,658 files.

Each host passes 578 dedicated constructor/ownership checks. The original comparison
matches 3,552 independent field values and 81 separately labelled scalar replay checks.
Actual native constructor entropy is qualified separately from independent JVM entropy.
The unchanged-original package executes 15 constructors per ordinary/JDI VM, verifies
258 checkpoints and 13 live method bodies, and pins the 182-class prepared registry
closure. Final JDI observations cover Slime sizes 1, 2 and 4; fresh native
declared-clock controls cover all three sizes independently.

At that checkpoint, focused Windows profiles continue from the returning frame into the actual
Mob spawn prefix. The five admitted hostile classes complete construction, yaw and
pose before eligibility at `cq.a:820`; other constructors remain at `cq.a:773`.
