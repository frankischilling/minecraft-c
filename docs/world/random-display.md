# Client random display updates

`World.q(III)V` samples the area around the actual local Player after the
World scheduler returns. Its caller captures the World first, then reads and
floors each current Player coordinate separately. A paused client skips the
call. This operation is separate from random Chunk simulation ticks.

Each call constructs one ordinary default Random from the same application
profile. That object stays distinct from `World.r` and survives through every
selected Block display call. Its constructor consumes the shared uniquifier
and a real clock sample even when all sampled blocks are air. It does not
derive a seed from the World or replace an existing Random owner.

The source loop performs exactly 1,000 samples. Each sample reloads `World.r`
for six `nextInt(16)` calls, then computes wrapping integer coordinates as
the supplied coordinate plus the first draw minus the second. All six draws
precede the actual acquiring Block getter, including samples outside the
World's height. The outer method changes neither `World.g` nor its cave-sound
cooldown. Block children may consume additional draws or emit effects.

Only a positive returned ID reaches the actual `Block.m` active use and
virtual display method. `Block.n` is absent from this method. Grass, Leaves,
Ice and Snow Block inherit the empty base display body; their simulation
methods do not run here. The sample counter advances only after its child
returns. A failure preserves the pending sample and all completed effects.

## Block effects

The original client has eleven declarations of this exact display method:
the base Block, Stairs, Redstone Ore, Fluid, Torch, Redstone Torch, Furnace,
Redstone Dust, Repeater, Portal and Fire. Dispatch follows the actual concrete
Block and inherited method. Stairs passes the same Random to its wrapped Block.

Redstone Ore uses a captured `World.r` for its six particle faces. Its lower
Y comparison is against zero, as in the original. Fluid's water-sound and
lava-particle checks are independent. Torches, Furnace, Redstone Dust and
Repeater preserve metadata orientation, exact float arithmetic and their
individual Random draw order. Portal consumes even the velocity draws that
its selected axis later overwrites. Fire tests each supported face separately
and emits every selected pair of particles.

Effects use the real World sound and particle dispatchers. These reread the
live WorldAccess List after each synchronous callback. Empty Lists return;
registered renderer and sound receivers execute their actual children.
Concrete smoke, flame, lava, portal, large-smoke and redstone particle
construction remains a dependency in RenderGlobal. Reaching such a child
preserves its real allocation frontier and preceding caller effects.

## Lifetime and failure

The retained display call owns its World and its local Random until native
retirement. The Block getter releases its local Chunk hold on return; this
outer method has no captured Chunk local spanning a display callback.
Acquisition, Registry, Block and WorldAccess failures retain their actual
source frames and causes. Callback failure receipts hold the producer
independently while the retained call is inspected.

Retirement releases native occurrences. It does not restore source fields,
reseed Random, replay draws, increment a failed sample or undo callbacks.
The client continues to EffectRenderer ticking and its existing wall-clock
store only after the display call returns.

## Original evidence

The [reproducible original package](../../tests/reference/world/random_display/README.md)
contains twelve World inputs and 99 direct Block inputs. All 111 ordinary and
read-only debugger records match byte for byte. Eight World inputs complete
all 1,000 samples; the others retain their actual failure prefixes. The
observer checks 24 live method bodies, 172 class identities and the same 171
initialized original classes in both processes. It records 609 Random returns,
257 query returns and 17 exception events.

Each World call constructs one actual local default Random. Its observed
uniquifier and clock pair independently reproduce its seed; those values are
never injected into either runtime. Direct Block controls instead use declared
seeded argument and World Random inputs. Exact comparisons cover all 99 direct
state rows, 159 direct events, twelve World state rows and 2,003 World events.
Positions and velocities use complete binary64 words; volume and pitch use
complete binary32 words.

Controls include all Torch/Furnace orientations, Repeater delay branches,
water and lava tickets, Portal axes, all five independent Fire faces, live
World Random replacement and callback failure. Integer-edge arguments check
wrapping neighbors and float-before-double rounding. Null selected Block and
World Random controls fail at their original invocation sites after the
proper constructor/draw prefix. Clearing the listener List still allows the
second Torch emission's World method to return without a listener callback.

Native-only controls separately cover a null whole Block table, unused null
World/Random arguments and 1,000 Redstone Torch calls using the fresh local
Random. A plain value replay checks its 3,000 float draws without rebinding
the live owner. Some older detailed getter failures remain native boundaries
where their producer exposes no source exception classification. Callback
adapter failure effects are compared separately from JVM Throwable identity.

## Native qualification

All 78 selected targets build on Windows MSVC Debug and Linux Clang18 ASan/UBSan.
All 93 Windows tests pass in 150.97 seconds and all 89 Linux tests in
209.98 seconds, without failures, skips or sanitizer findings. The same 2,441
C, helper, build and reference inputs remain unchanged through all four phases.
Formatting checks 1,685 files; source bindings cover 7,909 entries and 3,381 files.

Each host also passes all six current original-comparison suites: Mob
construction, spawning, Lightning construction, random Chunk work, concrete
Block simulation ticks and random display updates. Their native executables,
source inputs, original captures and compiled helper fingerprints are checked
without changing the reference inputs.

The loaded Windows controls preserve resources, save bytes and all 72 original
focus histories. A focused unpaused call completes 361 Chunk coordinates and
28,880 simulation samples, then all 1,000 display samples and 6,000 World
Random draws. EffectRenderer ticking returns. A subsequent actual Mob reaches
its unfinished active tick through `fd.a(Entity,boolean):121` at
`Minecraft.k:1216`; the final cohort observed a Chicken. Native diagnostic
counters retain completed calls after their child owners retire. They do not
control timing or gameplay. The genuine Skeleton jockey mount remains a
separate possible earlier dependency.
