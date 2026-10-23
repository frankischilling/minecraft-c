# Build and tests

Build setup uses C17, CMake 3.24+ and Python 3.10+; ordinary public tests need
no Java or original archives. Follow [building](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/building.md) for
Linux and Windows tools, dependency setup and separate build directories.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Code checkpoint `eee7fb0` passes 191 tests with MSVC 19.51, 193 with GCC 13.3
and 193 with Clang 18.1.3, with zero failures, errors or skips. Every compiler
includes all three fresh original population profiles and all three public native replays.
Linux enables ASan, UBSan and leak checks. All 550 C/header files pass
clang-format 18. Mirrors and lifecycle intervals pass on all three hosts,
including 556 positive and 2,011 negative interval controls.

The independent [source and capture audit](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-persistence.md#committed-checkpoint)
verifies 1,048 exported source files and all eighteen fresh/public population
captures, including both resource bindings for each compiler, side and profile.
The [hosted code run](https://github.com/frankischilling/minecraft-c/actions/runs/39945110219)
passes all 565 public/JDK tests: 187 MSVC, 189 GCC and 189 Clang. Formatting and
all mirror/interval checks pass; all four actual checkout trees match the code
commit. Four archive-dependent tests per compiler retain their local scope.

Optional `reference.natural_population` and `reference.natural_population_ring`
compare fresh original/native origin and ring profiles on both sides. Ring
retains nine normal Chunks, populates four and ends with 6,968 ordered light
regions. The [fresh World record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/natural-population.md)
defines the full cache, numerical-array, alias and Random projection.

Public `world.natural_population_replay` checks the older cache oracle;
`world.natural_population_ring_replay` also checks full original numerical
golden records. Both run the real native probe with declared minimal resources
and require no Java or archives. Select both fresh comparisons with
`-R '^reference\.natural_population(_ring)?$'` after enabling
`MC_ENABLE_POPULATION_REFERENCE_TESTS=ON`; the
[test commands](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#fresh-world-population)
include the separate public replay commands.

The same population reference flag also enables
`reference.population_save_readback`. Public
`world.population_save_readback_replay` needs no Java or archive, and
`tools.population_persistence` checks corruption controls. The
[save/readback record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-persistence.md)
defines its twenty complete boundaries, disk/NBT projection and remaining full
World save, unload and restart requirements.

The plant comparison has 59 client and 59 server cases, with zero mismatches. Its
[declared projection and exclusions](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/plant-lifecycle.md)
bound this result; it does not establish original World/Chunk parity.

Public `world.plant_lifecycle` and `world.plant_lifecycle_replay` need no Java.
Replay uses original-derived fixtures: 59 cases, twelve semantic corruption
guards and 154 malformed-input guards per side. For fresh captures, enable `MC_ENABLE_PLANT_REFERENCE_TESTS=ON`,
supply both verified JARs and select Java/javac from one JDK17+ installation.
The [test commands](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#plant-lifecycle-validation) list the
JAR, tool and tested-source options.

See [testing](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md), [development status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md),
the [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md) and
[World simulation](https://github.com/frankischilling/minecraft-c/wiki/World-simulation) for the remaining acceptance work.
The [plant contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/plants.md) separates implemented callbacks
from the unfinished game loop.

<a id="normal-generation-checks"></a>

The [complete former build page](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-builds.md), including its
[normal-generation checks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-builds.md#normal-generation-checks),
preserves older counts, commands and checkpoints as historical evidence.
