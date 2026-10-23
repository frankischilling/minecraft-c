# Build and tests

Build setup uses C17, CMake 3.24+ and Python 3.10+; ordinary public tests need
no Java or original archives. Follow [building](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/building.md) for
Linux and Windows tools, dependency setup and separate build directories.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Code checkpoint `6ad91ec` passes 186 tests with MSVC 19.51, 188 with GCC 13.3
and 188 with Clang 18.1.3, with zero failures, errors or skips. Linux runs include
ASan, UBSan and leak checks. Formatting passes for all 550 C/header files.
Mirrors and lifecycle intervals pass on all three hosts, including 556 positive
and 2,011 negative interval checks per host.

At code commit `6ad91ec`, the
[hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/39929133496)
passes 184 MSVC, 186 GCC and 186 Clang tests, 556 total. All four actual checkout
trees match the code commit; formatting, mirrors and lifecycle controls pass.
Hosted tests use the public fixtures. Fresh supplied-archive population/plant comparisons
retain their separate local scope.

Each full local suite includes fresh optional original population and plant
comparisons. The [fresh World result](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/natural-population.md)
matches the seed-23 origin neighborhood on both sides. The plant comparison has
59 client and 59 server cases, with zero mismatches. Its
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
