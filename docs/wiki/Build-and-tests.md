# Build and tests

Use a C17 compiler, CMake 3.24+, Ninja, and Python 3.10+ for tests. The current
baseline is x86-64 Linux and Windows. Public tests need no display, audio,
Minecraft assets, account, or JVM. Storage links statically to zlib 1.3.2;
initial configuration downloads its pinned, SHA-256-verified official archive.

On Linux install GCC or Clang, CMake, Ninja and Python. On Windows use Visual
Studio's x64 C developer shell. Then run:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

For a Visual Studio generator use `cmake -S . -B build -A x64`, build with
`--config Debug`, and add `-C Debug` to CTest. Keep separate directories for
different generators/compilers. Set `BUILD_TESTING=OFF` for a library-only build.
The [build guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/building.md)
contains complete setup and formatting commands; the
[dependency guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/dependencies.md)
explains offline source overrides and license notices.

Public checks cover numerical/text semantics, raw NBT, compression, both noise
families, climate/biome classification, normal density/base blocks, surface
replacement, normal cave carving, packed chunk arrays, height/skylight data
and full finishing, unpublished normal generation, metadata/schema ownership,
region image writing, native region transport, world-store acquisition and the
local resource importer.
GCC/Clang builds support `MC_ENABLE_SANITIZERS=ON`, including
the zlib dependency. CI runs native Windows MSVC and Linux GCC/Clang plus
clang-format18. No original JARs/assets are downloaded by CI.

`core/math` adds original float lookup/floor tests without Java. Its optional
`MC_ENABLE_MATH_REFERENCE_TESTS` checks every initialized table entry and
sampled scalar calls from both supplied originals; it needs no world setup.

`MC_ENABLE_JAVA_REFERENCE_TESTS=ON` adds the standard-library numerical oracle
with JDK8+. Original-class NBT/compression comparisons require JDK11+ and the
locally supplied verified client/server JARs, enabled separately through
`MC_ENABLE_NBT_REFERENCE_TESTS` and `MC_ENABLE_COMPRESSION_REFERENCE_TESTS`.
`MC_ENABLE_NOISE_REFERENCE_TESTS` adds exact original client/server noise
comparisons with the same supplied-file/JDK/source-commit requirements.
`MC_ENABLE_SIMPLEX_REFERENCE_TESTS` separately checks actual original 2D simplex
grid overloads, constants, mixed construction and octave controls.
`MC_ENABLE_BIOME_REFERENCE_TESTS` checks original classification and climate
methods, exact table contents and mutable buffer sequences, with client-only
raw temperature tested separately.
`MC_ENABLE_DENSITY_REFERENCE_TESTS` compares normal density/base methods in
genuine original world contexts created under fresh ignored directories. It
checks exact constructor state, climate, density, scratch and block values.
`MC_ENABLE_SURFACE_REFERENCE_TESTS` checks the untouched surface pass in
constructor-Random and explicit chunk-reseed contexts, including current
material bytes, complete fields/blocks, suffixes and live Random continuation.
`MC_ENABLE_CAVES_REFERENCE_TESTS` adds two comparisons: the public cave scheduler
in genuine base/surface contexts and protected room/path controls over owned
buffers. These need a JDK containing `java`, `javac` and `javap`. Both compare
complete buffers, Random continuations and preservation controls independently
between C/client, C/server and client/server.
`MC_ENABLE_NIBBLES_REFERENCE_TESTS` checks original packed-array constructors,
getter sweeps and setter snapshots. `MC_ENABLE_CHUNK_REFERENCE_TESTS` checks
fresh/repeated local chunk arrays and heights in genuine original world contexts.
The full corpus also needs frozen original post-cave inputs; see
[chunk reproduction](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/chunk.md).
`MC_ENABLE_LIGHT_REFERENCE_TESTS` checks actual original light-region merges
and world queues in genuine fresh worlds, preserving each starting queue. The
separate million-entry experiment observes the original automatic abort.
Public C tests replay recorded complete queues and storage contracts without
original files or Java. See the
[queue evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/lighting.md).
`MC_ENABLE_FINISH_REFERENCE_TESTS` adds full normal finishing with JDK17+ and
`jdk.jdi`. It observes actual ordered height entries/returns over two consecutive
passes in each of six fresh cases. A separate untouched control must match
complete snapshots exactly. The debugger uses loopback only. Public fixtures
compare all decoded bytes, queues and queries without Java; see
[finishing evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/finishing.md).

## Storage foundation checks

The normal build includes `mc_persistence`, the metadata codec, stored-chunk
owner, region image writer, native region cache and world store. Public checks
include `storage.level`, `storage.region_writer`, `world.stored_chunk`, the
`tools.level_*`, `tools.region_write_*`, `tools.chunk_storage_*` and
`tools.regions_*` checks, `tools.world_store_interop` and
`tools.world_storage_reference`. They use retained
observations and constructed C contracts without original archives or Java.

`MC_ENABLE_STORAGE_REFERENCE_TESTS=ON` enables `reference.level`,
`reference.region_write`, `reference.chunk_storage` and `reference.world_storage`.
These require both
locally supplied verified JARs, JDK17+ and `MC_REFERENCE_TESTED_COMMIT`. Keep
reference worlds in fresh owned or copied directories. The
[storage evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/storage.md)
distinguishes codec/image comparisons from native ownership and copied-world
acceptance; the
[ownership contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/storage.md)
describes lifetimes, limits and unsupported operations.

At storage checkpoint `9ad690d83592f127b9bb39d7233e8eef06fd0029`, all 71 selected
local tests pass without skips on native MSVC and WSL GCC/Clang, with Linux
ASan/UBSan. The suite includes 58 public tests, 12 original comparisons and the
numerical Java oracle. Times are 744.00, 1255.43 and 1256.05 seconds respectively.
Independent audits bind source, tools, executables/build inputs, original
captures and complete supported copied-world state/control/byte projections.
All 74 Java-free world-harness guards pass. Hosted checks pass 59 tests per
compiler and clang-format18 for 111 C/header files. See the
[exact evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/storage.md#exact-source-storage-checkpoint)
for capture/report hashes and excluded attempts. These finite profiles do not
establish gameplay save/unload scheduling, subtype simulation or a running C
server. Older checkpoint counts below retain their original source and scope.

## Normal generation checks

`world.normal_generator` passes 99073 public contract checks on native MSVC and
WSL GCC/Clang; sanitizer checks also pass. They cover borrowed ownership,
unpublished identities, repeated outputs, current materials, finite-coordinate
preflight and allocation-failure preservation. Four-seed original capture and
lossless public C replay pass eight inputs, 72 complete chunks and 73728 exact
height queries. Exact source `a08c12056a06261ed01d9f66999459f7ce9543b4` passes
all 41 selected local tests without skips on those toolchains. Each generation
comparison retains 1224 stage events, 144 marker inventories and eight complete
byte-equal untouched controls, with all three pairings passing independently.

`MC_ENABLE_GENERATION_REFERENCE_TESTS=ON` enables `reference.normal_generation`
with JDK17+ and `jdk.jdi`, both supplied JARs and the exact source commit setting.
It uses fresh actual original contexts, loopback debugging and separate
untouched controls. The corpus uses seeds 23, 24, 26 and 51, retaining
one generator across nine calls per side. It includes repeated, negative,
packed-key-alias, world-border and accepted cave-domain endpoint coordinates.
Complete data, queues, climate/scratch, 82 layers, separate Random continuations
and all 1024 height queries per call form the comparison boundary.

After configuring the reference inputs, use these production commands:

```sh
cmake --build build --target mc_normal_generation_probe
ctest --test-dir build -R '^tools\.normal_(reference|interop)$' --output-on-failure
ctest --test-dir build -R '^reference\.normal_generation$' --output-on-failure
```

Add `-C Debug` for multi-configuration builds. The public
`tools.normal_reference` and `tools.normal_interop` checks use synthetic guards
and retained original fixtures respectively, without Java or private inputs.
At the a08 checkpoint, the suite has 35 public tests, 36 with the numerical
Java oracle, or 41 with the selected original finishing, queue, chunk, nibble
and generation comparisons. [Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39079200002)
pass all 36 public/numerical tests per compiler on native Windows MSVC and Linux
GCC/Clang, plus formatting for 79 C/header files. These counts identify that
exact checkpoint; later cache/region additions need separate acceptance. See the
[generation contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/generation.md)
and [evidence boundary](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/generation.md).

## Acquisition and region checks

The cache coordinator adds 838 public contract checks. Its retained-provider
fixture replays three continuous original contexts, 20 operations, five generated
chunks and 5120 exact height queries. The public protocol and fixture checks need
neither Java nor original archives. `MC_ENABLE_CACHE_REFERENCE_TESTS=ON` enables
`reference.chunk_acquire` and `reference.chunk_callbacks` with JDK17+, `jdk.jdi`,
`jdk.jdwp.agent`, both supplied archives and the exact source commit setting.
The callback fixture adds four contexts, 12 calls, 394 lifecycle events and 24
inventories. Public checks cover 34 guards, four genuine C replays, four
constructed cases and 28 additional C guards. These replay observed cache/world
effects while C computes coordinator transitions. Exact source
`28a55b52c001c4c114a11ca7df844fc1f286d328` passes all 56 selected tests without
skips on MSVC/GCC/Clang with Linux sanitizers and independent audit. Hosted checks
pass 48 tests per compiler and formatting for 89 C/header files. See the
[cache evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/cache.md).
The separate [callback evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/cache-callbacks.md)
records saved-world preparation, ownership and explicit external-effect limits.

The read-only region reader has 49771 public checks. At source
`092301ae8282d50a734df9018f791925aa9be99b`, `reference.region` passes on native
MSVC and WSL GCC/Clang with Linux sanitizers. Each capture retains 36 fixtures,
49 queries per side and two byte-equal untouched controls. Eight queries are
compatible; 41 retain explicit bounded diagnostic departures.
[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39082649705)
pass 42 public/numerical tests per compiler and formatting for 87 C/header files.
See the [region evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/region.md).

Expanded source `9a50ac67997082c89dbc43cf4274fba1427c1565` passes all 52
selected local tests without skips on MSVC/GCC/Clang with Linux sanitizers and
independent exact-source audit. The suite contains 44 public tests, the numerical
Java oracle and seven selected original comparisons.
[Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39085300876)
pass 45 tests per compiler and formatting for 88 C/header files.

Missing references are unavailable/skipped results, never parity passes. Pin the
source commit when recording comparisons. See the
[testing guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/testing.md)
and [storage/reference overview](Storage-and-reference).

The prior finishing checkpoint passes 37 local tests per compiler on native
Windows/MSVC and WSL GCC/Clang, including original finishing, queue, chunk and
nibble comparisons. The Linux builds use ASan/UBSan. All six finishing cases
match independently in three pairings; complete untouched controls agree.
Its public CI passes 33 tests per compiler and formatting for 75 C/header files.
Exact tested commits, manifest hashes and integration results live in the
development status and linked evidence pages.

Original resources are never bundled. The
[resource guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/resources.md)
describes local import into a fresh ignored directory and its current limits.

There is currently no C game executable or C server to host. The storage
library's explicit save operations pass the finite storage comparisons above;
complete original-compatible gameplay save workflows remain unverified. Client controls
and Linux/Windows hosting, backup and shutdown instructions will be added with
verified game implementations.
