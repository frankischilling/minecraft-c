# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. There is no playable C client or server yet. The current libraries
cover compatibility primitives, storage, terrain generation and parts of World
simulation; the runtime does not require a JVM.

The latest complete native suites pass 175 tests on MSVC and 177 each on GCC and
Clang, with zero failures or skips. Linux runs use ASan/UBSan and leak checks;
formatting passes for 515 C/header files. Rail tests pass 814 checks across 144
literal real-World cases. A separate error-transport fixture passes 1,444 checks
across 94 constructed records. These results cover the specified native
contracts, including topology, detector queries and powered search.

A separate finite power comparison at source `9f9d3a2` matches 5,365 literal
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
