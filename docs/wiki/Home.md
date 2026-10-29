# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. Its native libraries cover compatibility primitives, storage,
terrain generation and parts of World simulation. There is no playable client
or dedicated server yet; the C runtime requires no JVM.

The [native client](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/startup.md)
now defaults to the constructed source App and initialized main menu. Its
[menu runtime](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/menu-runtime.md)
connects source events, no-World ticks, source menu drawing and Quit. Explicit
flags retain the diagnostic title and fixed-camera terrain inspection. Four
focused Windows tests pass, including 100 menu frames and 32 ticks with cleanup
and an inspected 854 by 480 capture. All 463 selected Windows CTests pass. Hosted CI, Linux and original-game
correspondence retain their separate source scopes. World entry and other menu
destinations are unfinished; full source run/PX/shutdown parity is unproven.

The earlier `8288290` [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40792366222)
passes all 452 GCC cases, but Clang has one Display-creation failure and Windows
stops on a compiler warning. Those results and the newer Windows menu checks
have separate scopes in [Build and tests](Build-and-tests).

Published predecessor `3ec9892` passes [all four jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40796960416):
434 Windows, 452 GCC and 452 Clang tests, 1,338 total, without failures or skips.
That result precedes the new default menu runtime.

Earlier hosted baseline `5d61f89` passes
[all four jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40746811719):
1,264 public/JDK cases, with 410 Windows and 427 per Linux compiler. That hosted
result predates the newer resource-thread and menu additions.
[Build and tests](Build-and-tests) separates hosted coverage, local graphical
startup, component checks and finite original comparisons.

[LocalPlayer NBT](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-nbt.md)
now reaches actual Inventory hooks, first-Player metadata saving and saved
Info.Player attachment. Lava CPU animation also matches fresh original runs on
both hosts. These finite component results leave normal World entry, gameplay
input/tick adoption, dynamic rendering and gameplay interactions unfinished. The next
session milestone is loading one World, walking, changing Blocks and saving/reloading.

- [Client startup](Startup) describes the normal constructor chain and remaining World-entry work.
- [World simulation](World-simulation) links the plant, Minecart, rail, power,
  Piston, movement and population contracts and their reference evidence.
- [Build and tests](Build-and-tests) links build instructions and validation scope.
- [Architecture](Architecture) describes the native systems and their boundaries.
- [Storage and reference work](Storage-and-reference) links persistence and
  reference-development documentation.

The repository's [current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
and [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
separate implemented behavior, native tests and finite original comparisons.
Detailed checkpoint records are in the
[population and runtime history](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/population.md);
[wiki history](History) preserves the earlier overview.
