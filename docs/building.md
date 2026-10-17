# Build and development setup

The working target is native x86-64 Linux and Windows, using C17 and CMake 3.24
or newer. There is no playable client or dedicated C server yet. Shared-library
builds and public tests need no display, graphics, audio, Minecraft assets, or
Java runtime. Storage uses pinned static zlib; see [dependency setup](dependencies.md)
for its verified source archive, license, and offline source override.

Install GCC or Clang, CMake, Ninja and Python 3.10+ on Linux. Build with:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows, install Visual Studio's C desktop tools, CMake, Ninja and Python.
Run the same commands from an x64 developer shell. Alternatively, use a Visual
Studio multi-configuration generator:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Choose a separate build directory when changing generators or compilers.
For GCC/Clang sanitizers, configure `-DMC_ENABLE_SANITIZERS=ON`; instrumentation
covers project code and its zlib dependency. Strict diagnostics apply to project
source. Set `-DBUILD_TESTING=OFF` to build libraries without Python/Java discovery.

Enable `-DMC_ENABLE_JAVA_REFERENCE_TESTS=ON` for the standard-library numerical
oracle; it requires JDK8+ and uses no Minecraft files. The NBT and compression
original-class comparisons are separate local checks requiring JDK11+ and both
verified supplied JARs. See [testing](testing.md), [NBT reproduction](reference/nbt.md)
and [compression reproduction](reference/compression.md). Missing private
references produce unavailable results, not successful comparisons.

`MC_ENABLE_NOISE_REFERENCE_TESTS=ON` separately enables the original Perlin/octave
comparison with the same JAR/JDK/commit settings. See [noise evidence](reference/noise.md)
and its [interface](world/noise.md). The `mc_world` runtime library needs neither
Java nor original files.

`MC_ENABLE_SIMPLEX_REFERENCE_TESTS=ON` adds the separate original 2D simplex
comparison, using the same supplied JAR/JDK/commit settings. Its
[interface](world/simplex.md) and [reference notes](reference/simplex.md) keep
noise primitives separate from complete biome and terrain generation.

Runtime code is organized as `src/<system>/<subsystem>/`, with matching public
headers under `include/mc/` and tests under `tests/`. See [architecture](architecture.md)
for current and planned modules. `.clang-format` sets four spaces, a 100-column
limit, and Linux-style braces. CI uses clang-format18:

```sh
python scripts/development/format_code.py --clang-format clang-format-18
python scripts/development/format_code.py --check --clang-format clang-format-18
```

On Windows, pass the installed formatter's executable path if needed. Formatter
input is restricted to project C sources/headers; ignored reference material is
not scanned.

Supply your own client JAR and resource directory to the local importer:

```powershell
python scripts/resources/import_assets.py --jar b1.7.3.jar --resources "$env:APPDATA/.minecraft/resources" --output .local/assets
python scripts/reference/binaries/inspect_jars.py --client b1.7.3.jar --server b1.7.3_server.jar --output .local/reference/jars.json
```

Use explicit Linux paths on Linux. The importer requires a fresh output
directory and reports missing resources. See [resource evidence](reference/resources.md)
and [binary setup](reference/binaries.md). No native asset consumer is implemented
yet. Keep originals, launcher files, credentials, recordings, logs and worlds in
ignored local directories. Copy a world before any writable reference test.

Current compiler/platform results and remaining graphical access limits are in
[the resume note](status.md). Client controls, compatible world writes and hosting
instructions will accompany actual implementations.
