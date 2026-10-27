# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. Its native libraries cover compatibility primitives, storage,
terrain generation and parts of World simulation. There is no playable client
or dedicated server yet; the C runtime requires no JVM.

The [native title frontend](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/startup.md)
draws imported resources. A separate fixed-camera inspection renders generated
terrain. The combined Timer, Minecraft class/worker and rendering source passes
1,163 local tests: 393 Windows and 385 per sanitized Linux compiler. All three profiles exercise
real graphical startup and fixed-camera World inspection.

[LocalPlayer NBT](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-nbt.md)
now reaches actual Inventory hooks, first-Player metadata saving and saved
Info.Player attachment. Lava CPU animation also matches fresh original runs on
both hosts. These finite component results leave normal World entry, input/tick
adoption, dynamic rendering and gameplay interactions unfinished. The next
session milestone is loading one World, walking, changing Blocks and saving/reloading.

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
