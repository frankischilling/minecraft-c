# Build and tests

The combined RenderEngine, Biped, Lava animation, LocalPlayer NBT, controller,
camera and World update source passes 1,151 local CTests: 389 Windows MSVC Debug
cases and 381 cases on each sanitized Clang/GCC Debug build with leak detection.
Every preceding 376/348/348 case remains enabled, with no skip or failure.
All three profiles exercise real OpenGL startup, title and World inspection.
Formatting passes for 926 C/header files. The tested cohort has 1,867 files and
1,627 non-Markdown inputs. Source and all configured products stay unchanged;
all 281 Windows executables retain their default 1 MiB stack reserve.

The [test record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md) separates the complete matrix from
individual operation profiles:

| Operation | Scoped evidence |
| --- | --- |
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

The preceding `0a330b2` [hosted matrix](https://github.com/frankischilling/minecraft-c/actions/runs/40435604059)
passes all four jobs and 1,016 public/JDK cases: 334 Windows and 341 per Linux
compiler. Every prior 980 case remains, with twelve new public Lava/NBT cases
per platform. All four checkout trees equal the reviewed
`78391e129eb8dca8453711bc6a4305d05c394e74` tree. Formatting checks 910 C/header
files; controller mirrors and all 556 positive/2,011 negative lifecycle interval
controls pass on each platform. Original-archive comparisons retain their
separate local scope. Hosted validation of the newer RenderEngine/Biped
integration is pending publication.

CI runs two CTests concurrently and allows four parallel Windows build jobs.
Linux build jobs cache downloaded APT package archives, then still install the
packages and run every compiler, sanitizer, graphics, formatting and controller
check. At `2f65ade`, both Linux jobs restored the cache, needed zero package-archive
download bytes and spent eleven seconds on dependency installation. Package
indexes were still updated. These observed timings do not establish a controlled
whole-run speedup. The earlier
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
