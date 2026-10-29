# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. Its native libraries cover compatibility primitives, storage,
terrain generation and parts of World simulation. There is no playable client
or dedicated server yet; the C runtime requires no JVM.

The [native client](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/startup.md)
now defaults to the constructed source App and initialized main menu. Its
[menu runtime](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/menu-runtime.md)
connects source events, no-World ticks, source menu drawing and Quit. Singleplayer
now opens genuine
[World selection](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/world-selection.md),
with real sorted saves, row selection and Cancel returning to the same parent.
Four native Windows profiles pass, including scrollbar drag and real double-click.
Empty, populated and bottom-scroll captures were inspected.
Select constructs the actual controller, then stops at World-load
`a(NULL World)`2. World loading and gameplay remain unfinished. Explicit flags
retain the diagnostic title and fixed-camera terrain inspection.

DateFormat matches 2,926 JDK17 vectors on Windows and Linux within its admitted
English/en_US, UTC/Eastern and 1967..2099 profile. Other profiles fail explicitly.
All 468 selected Windows CTests and formatting for 1,356 files pass. The
[`1334db5` hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40813601193)
passes Windows 440/440 and Clang 461/461; GCC passes 460/461, with a real Display
creation refusal in its later-owner constructor profile. Cleanup succeeds,
but the underlying SDL cause remains undiagnosed. A diagnostic-only follow-up
retains the captured error details. The preceding
[`de4290b` hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40802203908)
passes 435 Windows and 454 Clang tests; GCC passes 453/454, stopping before client
launch in its executable fingerprint guard. The current streaming-hash
correction passes its public Windows/Linux regression. Full results and the
unmeasured original-artifact size are recorded in [Build and tests](Build-and-tests).

The later [`d71bd94` run](https://github.com/frankischilling/minecraft-c/actions/runs/40815149868)
again passes Windows and Clang, with one GCC SDL video-init failure in SDK
transport. A private pinned-SDL reproduction captures `x11 not available`
under default Xvfb; adding only `-noreset` passes 2,400 repeated window cycles.
Linux CI now preserves its server between clients, keeping four workers and
all tests. The SDK test retains actual SDL refusal details. The
[matched evidence](Build-and-tests) keeps these local results separate from
the pending complete hosted verification.

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
