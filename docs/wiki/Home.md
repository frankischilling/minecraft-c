# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. The native runtime needs no JVM after local resource import.
The dedicated server remains unfinished.

The client constructs all eleven default [Mob kinds](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/mob-construction.md),
runs their [spawn predicates](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/mob-spawning.md), and dispatches their actual
[active tick chains](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/mob-active-tick.md) through genuine Living fields.
Shared environment, action, movement and travel work uses real World queries;
sight uses the existing World ray tracer. Finite returning and failure-prefix
scenarios are qualified independently of original constructor entropy.

The focused Windows profile loads a saved World, then constructs and
publishes a Sheep through the ordinary Entity APIs before further frames.
Its continuation completes random Chunk work over 361
coordinates and 28,880 samples, then all 1,000 random-display samples and 6,000
World Random draws. Repeated execution now completes the actual Mob
Living/Creature path and movement work through `fd.a(Entity,boolean):121`, then
returns from `Minecraft.k:1328`. Rendering reaches the actual Mob renderer
invocation at `n.a:516`; that renderer body remains unfinished. Terminal
cleanup releases genuine Path aliases, target cycles and Piston-only roots.
A selected Spider jockey retains its separate unfinished mount.

Single-player remains pre-playable. Mob combat, storage and drawing,
further Block overrides, concrete display particles, sleeping wake transitions
and gameplay actions remain unfinished. Active achievement and tutorial notices
also retain an unfinished draw child. Focus-loss Canvas integration and the
hidden pause-screen save path remain open.

Focused Windows MSVC Debug and Linux Clang18 ASan/UBSan runs pass the actual
World path wrappers, shared ChunkCache, PathFinder and shared-cursor Path.
Each host passes 16,995 native checks over 64 paired scenarios: 143,871 typed
graph/state checks, six separate failed-Path constructor checks, 12,464 ordered
query attempts and 87,246 query words. Seven corrupted comparisons are rejected.
Eight native ceilings and actual shared-Path/outliving-World controls pass.

Mob following adds four full returns to the previous source-prefix cohort.
Each host passes 113,936 native checks, 19,832 typed field checks, 14,558
identity relations and all 72 events over 62 scenarios. The split is 52 full
returns, two callback failures, four source-null failures and four remaining
drop/hurt prefixes. All thirteen native controls and six reference negatives
pass. Constructor entropy remains independent.

At `f510367`, all 90 selected targets build on Windows MSVC Debug and Linux Clang18 ASan/UBSan.
All 117 Windows tests pass in 220.67 seconds and all 112 Linux tests in
264.30 seconds, without failures, skips or sanitizer findings. The same 2,508
C, helper, build and reference inputs remain unchanged through both builds and tests.
All eight original-comparison suites pass on each host. Formatting checks 1,722
files; source bindings cover 8,107 entries across 3,450 current files.

Detailed source order,
ownership and limits are in [client pathfinding](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/pathfinding.md) and
[Mob active ticks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/mob-active-tick.md). These scoped results do not
establish a playable client.

- [Startup and current client paths](Startup)
- [Build instructions, test results and scope](Build-and-tests)
- [World simulation](World-simulation)
- [Storage and original-reference evidence](Storage-and-reference)
- [Architecture](Architecture)
- [Earlier checkpoints](History)
