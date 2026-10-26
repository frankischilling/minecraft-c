# Build and tests

The combined singleplayer controller, camera, World Entity updates, Player
spawn/reset and weather source passes 1,032 local CTests: 362 Windows MSVC Debug
cases and 335 cases on each sanitized Clang/GCC Debug build with leak detection.
Every preceding 357/331/331 case remains enabled, without skips or failures.
Windows includes OpenGL startup, World inspection and camera comparisons; local
Linux coverage is headless. Formatting passes for 897 C/header files. Source
inventories and configured build products stay unchanged through each full suite.
Hosted public/JDK checks run separately.

The [test record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md) separates the complete matrix from
individual operation profiles:

| Operation | Scoped evidence |
| --- | --- |
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

At the preceding published checkpoint `2f65ade`, all 1,019 local cases pass:
357 Windows and 331 per sanitized Linux compiler. Its separately retained
[hosted matrix](https://github.com/frankischilling/minecraft-c/actions/runs/40395106185)
passes 968 public/JDK cases: 318 Windows and 325 per Linux compiler. Those dated
results predate the current controller integration.

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
