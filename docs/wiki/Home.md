# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. There is no playable C client or server yet. The current libraries
cover compatibility primitives, storage, terrain generation and parts of World
simulation; the runtime does not require a JVM.

Current Minecart inventory, semantic NBT and native storage have separate
native contracts: [inventory contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/minecart-inventory.md) and [native validation](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/minecart-inventory-nbt.md#native-validation).
Full local suites pass 180 MSVC and 182 GCC/Clang tests, 544 total, without
failures, errors or skips. These results are separate from the earlier
`39edf12` hosted checkpoint and finite constructor comparison. Original/native
inventory, NBT and storage equality remains pending.

## Previous committed checkpoint

At `39edf12`, hosted validation passes all 177 MSVC and 179 GCC/Clang tests,
with zero failures, errors or skips, plus 527-file formatting. Linux uses
ASan/UBSan and leak checks. The separate native rail contracts pass 814 checks
across 144 literal real-World cases; error transport passes 1,444 checks across
94 constructed records. Their scope covers topology, detector queries and
powered search.

A separate [constructor comparison](World-simulation#minecart-construction-and-detection)
matches ten null-World inputs per client/server side between the original
programs and Windows MSVC at `39edf12`.

The earlier finite power comparison at source `9f9d3a2` matches 5,365 literal
rows per client/server side against the original programs. It compares seven
snapshot fields, completion and normal-return booleans under the declared
rules. It does not establish original-runtime rail equality or whole-game
parity.

Start with [World simulation](World-simulation) for the current scope and links
to the rail, power and population contracts. [Build and tests](Build-and-tests)
links the build instructions and current validation record. The
[parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
records what each result establishes and what remains unfinished.

[History](History) preserves the earlier Home and navigation. The longer wiki
pages retain historical checkpoint material; their opening notes point to the
current documentation.
