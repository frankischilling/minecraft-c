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
and the local resource importer.
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
Missing references are unavailable/skipped results, never parity passes. Pin the
source commit when recording comparisons. See the
[testing guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/testing.md)
and [storage/reference overview](Storage-and-reference).

The recorded chunk source passes 30 local tests per compiler on native
Windows/MSVC and WSL GCC/Clang, including both original comparisons. Public CI
runs 28 tests per compiler and formatting for 61 C/header files. Exact tested
commits, manifest hashes and integration results live in the development status
and linked evidence pages.

Original resources are never bundled. The
[resource guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/resources.md)
describes local import into a fresh ignored directory and its current limits.

There is currently no C game executable, supported save writer, or C server to
host. Client controls, original-compatible save workflows, and Linux/Windows
hosting/backup/shutdown instructions will be added with verified implementations.
