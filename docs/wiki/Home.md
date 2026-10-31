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

The focused loaded Windows continuation completes random Chunk work over 361
coordinates and 28,880 samples, then all 1,000 random-display samples and 6,000
World Random draws. Repeated execution now enters a Mob's Living/Creature body
and reaches a genuine path request at `ii.E:153` or `ii.f_:44/156`, through
`Minecraft.k:1216` and `fd.a(Entity,boolean):121`. Terminal cleanup releases real
target aliases/cycles and Piston-only roots without repairing the failed tick.
A selected Spider jockey retains its separate unfinished mount.

Single-player remains pre-playable. Mob paths, combat, storage and drawing,
further Block overrides, concrete display particles, sleeping wake transitions
and gameplay actions remain unfinished. Focus-loss Canvas integration and the
hidden pause-screen save path remain open.

All 87 selected targets build on Windows MSVC Debug and Linux Clang18 ASan/UBSan.
All 111 Windows tests pass in 185.60 seconds and all 107 Linux tests in
221.09 seconds, without failures, skips or sanitizer findings. The same 2,473
C, helper, build and reference inputs remain unchanged through both builds and tests.
Formatting checks 1,702 files; source bindings cover 8,002 entries and 3,414
current files.

Each host passes 113,477 Mob checks over 62 scenarios and twelve native
controls. Original comparisons verify 19,800 typed field and state checks,
14,476 cross-phase reference identity relations, and all 72 events. It distinguishes
48 full returns, two callback failures, four source-null failures and eight
missing-child prefixes. Every selected child operand and twelve sight-ray
argument words match. Six deliberately wrong reference mutations are rejected.
Constructor entropy remains independent and unnormalized.

- [Startup and current client paths](Startup)
- [Build instructions, test results and scope](Build-and-tests)
- [World simulation](World-simulation)
- [Storage and original-reference evidence](Storage-and-reference)
- [Architecture](Architecture)
- [Earlier checkpoints](History)
