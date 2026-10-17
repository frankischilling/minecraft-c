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

Public checks cover numerical/text semantics, raw NBT, compression, Perlin/octave noise and the local
resource importer. GCC/Clang builds support `MC_ENABLE_SANITIZERS=ON`, including
the zlib dependency. CI runs native Windows MSVC and Linux GCC/Clang plus
clang-format18. No original JARs/assets are downloaded by CI.

`MC_ENABLE_JAVA_REFERENCE_TESTS=ON` adds the standard-library numerical oracle
with JDK8+. Original-class NBT/compression comparisons require JDK11+ and the
locally supplied verified client/server JARs, enabled separately through
`MC_ENABLE_NBT_REFERENCE_TESTS` and `MC_ENABLE_COMPRESSION_REFERENCE_TESTS`.
`MC_ENABLE_NOISE_REFERENCE_TESTS` adds exact original client/server noise
comparisons with the same supplied-file/JDK/source-commit requirements.
Missing references are unavailable/skipped results, never parity passes. Pin the
source commit when recording comparisons. See the
[testing guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/testing.md)
and [storage/reference overview](Storage-and-reference).

Original resources are never bundled. The
[resource guide](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/resources.md)
describes local import into a fresh ignored directory and its current limits.

There is currently no C game executable, supported save writer, or C server to
host. Client controls, original-compatible save workflows, and Linux/Windows
hosting/backup/shutdown instructions will be added with verified implementations.
