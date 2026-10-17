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

`MC_ENABLE_MATH_REFERENCE_TESTS=ON` compares the complete initialized float
lookup table and sampled sine/cosine/double-floor calls with both supplied
originals. It uses the same JAR/JDK/source-commit settings and initializes no
worlds. See [math evidence](reference/math.md) and its
[ownership/interface](core/math.md). Ordinary math tests require no Java.

`MC_ENABLE_NOISE_REFERENCE_TESTS=ON` separately enables the original Perlin/octave
comparison with the same JAR/JDK/commit settings. See [noise evidence](reference/noise.md)
and its [interface](world/noise.md). The `mc_world` runtime library needs neither
Java nor original files.

`MC_ENABLE_SIMPLEX_REFERENCE_TESTS=ON` adds the separate original 2D simplex
comparison, using the same supplied JAR/JDK/commit settings. Its
[interface](world/simplex.md) and [reference notes](reference/simplex.md) keep
noise primitives separate from complete biome and terrain generation.

`MC_ENABLE_BIOME_REFERENCE_TESTS=ON` compares actual original climate methods,
the initialized biome table and mutable buffer sequences using the same local
JAR/JDK/commit settings. See its [interface](world/biome.md) and
[reference evidence](reference/biome.md). These numerical fixtures do not start
an original world or establish terrain generation.

`MC_ENABLE_DENSITY_REFERENCE_TESTS=ON` compares original normal density and
base-block methods with the same supplied-JAR/JDK/source settings. Its local
fixture constructs genuine original world contexts in newly owned ignored
directories; the server path can create a session lock and directories during
normal setup. See [the interface](world/terrain.md) and
[setup/evidence](reference/density.md). Public tests need no original setup.

`MC_ENABLE_SURFACE_REFERENCE_TESTS=ON` compares original normal surface
replacement and explicit chunk-reseed contexts with the same JAR/JDK/source
settings. It creates fresh original contexts and records live Random separately
from constructor state. See the [interface](world/surface.md) and
[reference notes](reference/surface.md); complete chunks remain outside this pass.

`MC_ENABLE_CAVES_REFERENCE_TESTS=ON` compares the normal cave stage with the same
JAR/source settings and a JDK containing `java`, `javac` and `javap`. The natural
comparison prepares actual base/surface inputs in fresh original contexts,
then checks the separate cave Random and complete retained state. A separate
`reference.caves_geometry` test invokes the actual protected room/path helpers
on 44 owned-buffer controls without constructing worlds. See the
[cave contract](world/caves.md) and [evidence](reference/caves.md).

`MC_ENABLE_NIBBLES_REFERENCE_TESTS=ON` compares the original packed-array
constructors, complete getter sweeps and setter snapshots.
`MC_ENABLE_CHUNK_REFERENCE_TESTS=ON` compares fresh and repeated chunk-local
height/skylight results with both originals in separately owned world contexts.
Both use the supplied JAR/JDK/source-commit settings. The full chunk corpus also
requires the frozen original post-cave inputs described in
[chunk reproduction](reference/chunk.md). Public chunk tests use recorded
fixtures and require no original files or Java.

`MC_ENABLE_LIGHT_REFERENCE_TESTS=ON` compares original light-region merges and
ordered queue storage in genuine fresh worlds. It preserves each setup baseline
and separately observes the original million-entry abort. C storage begins
after world admission; full finishing and propagation remain open. See the
[queue contract](world/lighting.md) and [evidence](reference/lighting.md).

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
