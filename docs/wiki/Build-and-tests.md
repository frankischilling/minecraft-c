# Build and tests

The combined Timer, Minecraft class/worker, rendering and World/Player source
passes 1,163 local CTests: 393 Windows MSVC Debug cases and 385 cases on each
sanitized Clang/GCC Debug build with leak detection. Every preceding 389/381/381
case remains enabled, with no skip or failure. All profiles exercise real
OpenGL startup, title and World inspection. Formatting passes for 936 C/header
files; the tested cohort contains 1,885 files and 1,643 non-Markdown inputs.
Source and all products remain unchanged; all 283 Windows executables retain
their default 1 MiB stack. Documentation updates preserve pinned inputs.

The [test record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md) separates the complete matrix from
individual operation profiles:

| Operation | Scoped evidence |
| --- | --- |
| Ordered Timer | 1,267 constructor checks per compiler, existing arithmetic/clock controls; separate original eight-rate/ten-field qualification |
| Minecraft class and kg worker | 225 native checks and 70 normal/optimized controls per compiler; five original class/worker projections on prepared receivers, no instance-constructor claim |
| RenderEngine | 8,333 CPU/223 device/55 separate trace checks; 19 normal/optimized controls; fresh original 4,096-pixel/ten-glyph comparisons on both hosts and ten original GL rows on Windows |
| Biped construction | 4,381 checks/435 allocation prefixes; 128 normal/optimized driver controls and 838 fresh original records per compiler |
| LocalPlayer NBT and first-Player metadata | 490 NBT, 110 coordinate, nine diagnostic and 169 publication checks per compiler; 33 original cases and 1,302 normal tags. Fresh Windows originals, retained Windows observations for current Linux executables; separate 37-record wake prefix contains 33 tags, with full wake unfinished |
| Lava CPU animation | 132,659 native checks per compiler; fresh Windows/Linux originals match all 348 records across 23 cases and 31 calls |
| Singleplayer controller | 230 normal and 256 allocator checks per compiler; twenty original records per target, with fresh original execution on Windows |
| World Entity/TileEntity updates | 881 native checks and 54 exact original records per compiler |
| Weather method | 473 native checks, unchanged 8,565-check Info regression and 68 exact original records per compiler |
| Player spawn/reset | 270 native checks and 273 allocator checks; Windows compares 42 reset records and 36 position vectors |
| Selected camera | 438 native checks and 1,198 callback checks per profile; declared Windows original matrix comparison |
| Pickup particle and CPU manager | 4,571 native checks; 71 exact original records with 4,592 paired checks |
| Manager ownership | 12,095 native checks |
| Complete-App pickup/lifetime | 82 native checks |
| Active Player/Input/allocation | 678 active, 1,222 Input and 247 allocator checks |
| Active animation arithmetic | 8,676 JDK atan2 results |
| Water animation | 103,098 native checks and the separate exact original array/RGBA comparison |
| Item tick | 1,089 native checks and 70 original records: 37 states, 33 complete calls and four prefixes |

Earlier isolated profiles retain their own coherent products. The genuine active-App original
profile retains its Windows LWJGL scope. Method comparisons do not establish a
World gameplay loop, fog, normal RenderEngine startup, drawing or a playable
client. See [active Player ticking](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-active-tick.md),
[pickup/effects](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/particles.md) and [Item ticking](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/item-tick.md).

The preceding published `9318923`
[hosted matrix](https://github.com/frankischilling/minecraft-c/actions/runs/40452503725)
passed all four jobs and 1,043 public/JDK cases: 341 Windows and 351 per Linux
compiler. All 1,016 preceding cases remained. All four checkout trees matched
the reviewed `d5ac89ab96384b1bfc2e5e5991cbf85c958a4a8c` tree. Formatting checks 926 C/header files,
controller mirrors and 556 positive/2,011 negative lifecycle controls passed
per platform. Hosted validation of the new Timer/class/worker integration is
pending publication; original-archive comparisons retain their local scope.

CI runs two CTests concurrently and allows four parallel Windows build jobs.
At `9318923`, both Linux jobs restored 9,538,264 cache bytes and downloaded 1,789 kB
of 11.4 MB required package archives. Dependency installation took eleven/twelve
seconds; package indexes were still updated. All compiler, sanitizer, graphics,
formatting and controller checks remained enabled. These observations do not
establish a controlled whole-run speedup. The earlier
[scheduling comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#continuous-integration-scheduling)
retains its separate measurements.

Build with C17, CMake 3.24+ and Python 3.10+. Client audio also needs a C++20
compiler for OpenAL Soft; project-owned runtime remains C17. Ordinary native
public tests require neither Java nor original archives. Follow
[building](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/building.md) for Windows/Linux tools, offline overrides and
separate build directories.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Optional reference tests require the locally supplied, verified archives and
the dependencies declared by each profile. Missing prerequisites do not count
as parity passes. The [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md) and
[development status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md) describe the remaining acceptance work.

<a id="normal-generation-checks"></a>

The [former build page](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-builds.md), including its
[normal-generation checks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-builds.md#normal-generation-checks),
retains older commands, counts and their original scope. Current reproduction
commands and finite comparison limits are in the [test record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md).
