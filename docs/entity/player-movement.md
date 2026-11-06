# Local Player movement and walking effects

The base mover now admits a genuinely constructed canonical LocalPlayer in
addition to Item and Falling Block. It moves the permanent bounds, retains the
actual pooled copies and mutable collision List, and preserves completed stores
when a descendant fails. It does not reset either geometry pool or advance a
Living or World tick.

`mc_entity_move_in_context` receives the application's actual loaded Registry
namespace separately from the nullable World. That context is read only when a
source static operation is reached. In particular, no-clip returns without
reading World or initializing Block or statistics. The existing `mc_entity_move`
entry remains available with no explicit context; its Item and Falling behavior
is unchanged.

The true-walking interval follows `sn.b(DDD)V` at1378..1677. It evaluates the two
position differences before the walking virtual, preserves the square/add and
sqrt-to-float boundaries, and increments the step counter before sound or
step-on dispatch. Support, Fence, Snow Layer and Material reads stay separate.
The Fence branch makes its third acquiring block read and passes the original
Y coordinate to the eventual Block method. The Block table is read again after
the sound callback, so synchronous changes remain visible.

StepSound implements the complete base, Glass and Sand method family. The Glass
and Sand overrides apply to `a()`; every `d()` still constructs a fresh
`step.` name. A sound name retains its actual String identity through the whole
WorldAccess iteration. A recipient can retain that same identity after the
call. The last release frees both the payload and its managed allocation record.
Interned subclass literals remain in the defining Registry's existing pool.

The separate `Block.b(World,III,Entity)` step-on dispatcher covers all69 current
native kinds. The original binary graph has73 Block classes on each side and
five declarations of this overload: base, Leaves, Redstone Ore, Stairs and
Farmland. Leaves invokes the empty inherited base body. Stairs delegates to its
actual wrapped Block. Redstone Ore performs six ordered neighboring opacity
queries, eighteen actual World Random draws, its source particle conditions and
then the notifying activation setter. Its Y particle comparison against zero is
preserved. Farmland uses the existing concrete trampling helper. A new native
kind requires another inheritance audit; an unknown kind never succeeds as base.

LocalPlayer effects use the real dispatch. Sneaking reads MovementInput.sneak
and then Player.sleeping. Walking resolves to inherited true. Portal contact
updates Player.z/A. Small falls execute Player, Living and base bodies in order.
Fall distance resets only after the delegated callback completes.

Larger falls, nonzero damage and retirement have explicit reached frontiers.
A fall of at least 2 resolves actual `jl.n`, computes the Java-rounded amount
and invokes the [real LocalPlayer statistics/application bridge](../client/player-statistics.md).
Successful dispatch continues through the Living/base fall body. A fall of 2.5
blocks completes; a nonnull passenger retains the increment before its recursive
virtual dependency. Player damage preserves its av, health, sleep, Class and
difficulty prefix, increments actual `jl.x`, then reaches the unfinished Living
hurt invocation at gs158. Wake, Arrow attacker ownership and other Living ally handling have
separate failures. Later Living hurt, armor, landed sound and death are not
implemented here. Player.K stores be through its superclass, then fails at the
first actual inventory Container close call. None of these failures is a
successful empty damage, fall or close body.

The ordinary native test needs no JDK or original assets. It constructs real
Registry, GAME World, loaded Chunk, Session, LocalPlayer and MovementInput owners
using the existing authored resource and NBT fixture. It checks clipping,
walking, sound callback failures, small falls and explicit dependency prefixes,
the complete step-on family, native limits and reclaimable String ownership.
Repeated allocations check both managed charge and permanent Registry charge.
The existing Item/Falling movement test remains a separate regression.

The optional reference controller executes an unchanged supplied client JAR
with actual World, Chunk and LocalPlayer constructors. It compares21 exact
records for StepSound methods/identity, six real moves, portal, small fall and
early damage. Twelve additional records compare reached fall-statistic amounts
against the actual JDK17 Math.round operation; these are component checks, not
completed original statistics dispatch. Scalar fields, Chunk cells and a
post-constructor World Random.setSeed are explicit authored inputs. Constructor
entropy, full World arrays, arbitrary WorldAccess graphs, all movement inputs,
Living travel and a complete Player tick are outside this finite comparison.

The controller requires matching explicit JDK17 tools, the pinned client JAR and
four pinned dependency JARs. It needs no LWJGL native library or display. Every
subprocess has a timeout and separate byte limits. Inputs and compiled helpers
are hashed before and after execution; class and Code metadata are checked
against source-pins.json. Worlds, runtime outputs and class files stay in the
chosen private work directory. No original binaries, assets or captured answers
belong in the source package.
