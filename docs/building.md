# Build and development setup

The working target is native x86-64 Linux and Windows, using C17 and CMake 3.24
or newer. There is no playable client or dedicated C server yet. The default
build includes a native graphical title screen and fixed generated-world
inspection; see [client startup](client/startup.md) and
[World rendering](client/world-renderer.md) for their scope and resources.
Client compilation needs SDL's platform development dependencies, but ordinary
tests need no display or original assets. Use `-DMC_BUILD_CLIENT=OFF` for shared
libraries and tests without client dependencies. See
[dependency setup](dependencies.md) for pinned archives, licenses and offline
source overrides. Java is needed only by enabled reference tests.

Install GCC/G++ or Clang/Clang++, CMake, Ninja and Python 3.10+ on Linux.
OpenAL Soft needs C++20; project-owned runtime code remains C17. Select matching
C and C++ compiler families when enabling sanitizers, for example
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`.

For Ubuntu/Debian client builds, install the X11/OpenGL and audio development
headers used by SDL:

```sh
sudo apt-get install pkg-config libasound2-dev libpulse-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
  libxi-dev libxss-dev libxtst-dev libgl1-mesa-dev
```

The list follows [SDL's Linux build notes](https://github.com/libsdl-org/SDL/blob/release-3.4.18/docs/README-linux.md).
Other distributions use their corresponding packages. Configure and build with:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Use the Linux filesystem for source and build trees when validating under WSL.
Source-binding tests perform many small file operations; mounted Windows sources
can exceed their existing deadlines. See [recorded test scope](testing.md).

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

Client builds also instrument SDL, libpng, Ogg/Vorbis and OpenAL Soft with
GCC/Clang sanitizers. `MC_BUILD_CLIENT_AUDIO` defaults to `MC_BUILD_CLIENT` and
requires `MC_BUILD_CLIENT_SOUND`. It adds the shared OpenAL device backend and
its real loopback test. Set audio off for codec-only builds; fresh headless
configurations leave both audio and graphical dependencies disabled.
`MC_ENABLE_CLIENT_DISPLAY_TESTS=ON` adds `client.display` and
`client.world_renderer` integration tests.
It creates hidden windows with real OpenGL contexts and fails if the desktop
cannot provide OpenGL 3.3 core. It uses authored pixels and needs no Minecraft
resources. Keep this option off on runners without a graphics environment.

`client.movement_input` and `entity.turn` build without graphics or audio.
Optional `MC_ENABLE_INPUT_REFERENCE_TESTS=ON` and
`MC_ENABLE_TURN_REFERENCE_TESTS=ON` compare their configured native observers
with the supplied client JAR. Set `MC_REFERENCE_CLIENT_JAR` and select Java/Javac
from one JDK17+ installation. These CPU observations construct no Player and
do not establish the client gameplay loop. See [MovementInput](client/movement-input.md)
and [Entity turning](entity/turn.md).

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

Plant lifecycle contracts and replay of recorded original results are part of
the normal test build and need no Java or original archives. Set
`MC_ENABLE_PLANT_REFERENCE_TESTS=ON` to add `reference.plant_lifecycle` with both
verified `MC_REFERENCE_CLIENT_JAR` and `MC_REFERENCE_SERVER_JAR` paths. Select
`Java_JAVA_EXECUTABLE` and `Java_JAVAC_EXECUTABLE` from the same JDK17+ installation,
and record the source with `MC_REFERENCE_TESTED_COMMIT`. See the
[test commands](testing.md#plant-lifecycle-validation), [runtime contract](world/plants.md)
and [comparison scope](reference/plant-lifecycle.md). The comparison runs original
Block bodies with prepared World services; it does not start an original World.

`MC_ENABLE_POPULATION_REFERENCE_TESTS=ON` adds `reference.natural_population`,
`reference.natural_population_ring` and `reference.population_dedicated` with
those same archive and JDK settings.
The origin and ring tests construct genuine fresh original and native GAME
Worlds for seed23. The origin profile populates one of four normal Chunks; the
ten-call ring profile populates four of nine and includes negative coordinates. The
[comparison record](reference/natural-population.md) defines the complete cache,
numerical-array and Random projection and its remaining limits.

```sh
ctest --test-dir build --output-on-failure -R '^reference\.natural_population(_ring)?$'
```

Public `world.natural_population_replay` and
`world.natural_population_ring_replay` run the real native probe without Java or
archives, using project-authored minimal resource files. Origin checks the older
cache oracle and native numerical transports; ring also checks full original
numerical golden records and array aliases. Both population drivers default to
`--profile origin` and accept `--profile ring`. See the
[test commands](testing.md#fresh-world-population) for both public replays.

Public `tools.population_dedicated` and `world.population_dedicated_replay` also
need no Java or archives. The dedicated replay runs the production probe with
minimal resources and measured constructor inputs. Its separate optional
reference test captures and audits the supplied original before comparing a
fresh native run with exact original resources. See the
[dedicated commands and scope](testing.md#dedicated-construction-and-synchronous-fluid-recursion).
The [clean dedicated checkpoint](reference/dedicated-world-construction.md#committed-checkpoint)
passes 622 local tests on MSVC/GCC/Clang and 601 hosted public/JDK tests; the
seven supplied-archive comparisons per compiler retain their local scope.

`MC_ENABLE_DENSITY_REFERENCE_TESTS=ON` compares original Overworld density and
base-block methods with the same supplied-JAR/JDK/source settings. Its local
fixture constructs genuine original world contexts in newly owned ignored
directories; the server path can create a session lock and directories during
normal setup. See [the interface](world/terrain.md) and
[setup/evidence](reference/density.md). Public tests need no original setup.

`MC_ENABLE_SURFACE_REFERENCE_TESTS=ON` compares original Overworld surface
replacement and explicit chunk-reseed contexts with the same JAR/JDK/source
settings. It creates fresh original contexts and records live Random separately
from constructor state. See the [interface](world/surface.md) and
[reference notes](reference/surface.md); complete chunks remain outside this pass.

`MC_ENABLE_CAVES_REFERENCE_TESTS=ON` compares the Overworld cave stage with the same
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

`MC_ENABLE_FINISH_REFERENCE_TESTS=ON` compares full normal finishing, all 1024
ordered height queries per pass, dirty state and complete queues with both
untouched originals. It requires JDK17+ with `jdk.jdi` and the same supplied
JAR/source settings. JDWP binds to loopback only. Each of six cases has a fresh
world, two consecutive passes and a separate untouched control whose complete
snapshots must match. Public fixtures replay the full captured records without
Java. See the [finishing contract](world/finishing.md) and
[reproduction procedure](reference/finishing.md).

`MC_ENABLE_GENERATION_REFERENCE_TESTS=ON` enables `reference.overworld_generation`
for the complete unpublished Overworld generator entry. Build target
`mc_overworld_generation_probe` replays imported original world/cache/manager
baselines through the production C API. The comparison requires both supplied
JARs, JDK17+ with `java`, `javac`, `javap` and `jdk.jdi`, and the exact source
commit setting. It uses loopback debugging, fresh original worlds and separate
untouched controls. The acceptance corpus has seeds 23, 24, 26 and 51,
with nine sequential calls per retained generator. Complete exact-source
comparisons pass at
`a08c12056a06261ed01d9f66999459f7ce9543b4`; see
[generation evidence](reference/generation.md) and the
[ownership and failure contract](world/generation.md).

After configuring the JAR paths and source commit, the production commands are:

```sh
cmake --build build --target mc_overworld_generation_probe
ctest --test-dir build -R '^tools\.normal_(reference|interop)$' --output-on-failure
ctest --test-dir build -R '^reference\.overworld_generation$' --output-on-failure
```

Add `-C Debug` for a multi-configuration build. The two public tool checks use
synthetic guards and retained fixtures respectively; they need no original
JARs or Java. At the a08 checkpoint, all 41 selected local tests pass without
skips on native MSVC and WSL GCC/Clang, with ASan/UBSan on both Linux builds.
That suite contains 35 public tests, the numerical Java oracle and five
original comparisons. [Hosted checks](https://github.com/frankischilling/minecraft-c/actions/runs/39079200002)
pass 36 public/numerical tests per compiler and formatting for 79 C/header
files. Later cache/region work on the active branch requires separate results;
these counts identify the verified checkpoint. `mc_world` itself has no Java
or original-file dependency.

Cache acquisition adds `tools.acquire_reference`, `tools.acquire_fixture` and
`tools.acquire_interop`; all three pass on MSVC/GCC/Clang without Java or original
files. They retain 20 continuous calls and 5120 original query results. The
separate `mc_chunk_callback_probe` and `tools.callbacks_*` tests cover successful
loading and natural population at explicit callback seams. Enable
`MC_ENABLE_CACHE_REFERENCE_TESTS` for both live `reference.chunk_acquire` and
`reference.chunk_callbacks`. See [cache evidence](reference/cache.md) and
[callback boundaries](reference/cache-callbacks.md) for constructor routes,
limits, opaque callback effects and exact-source acceptance status.

The normal build includes `mc_region_inspect`. Enable
`MC_ENABLE_REGION_REFERENCE_TESTS` for `reference.region`; exact source
092301ae passes all 49 queries per side and untouched controls on all three
toolchains. [Region evidence](reference/region.md) records the eight compatible
cases, 41 diagnostic departures, independent audit and hosted checks.

The normal build also includes `mc_persistence`, composing native world files,
metadata, stable chunk owners and region transport. Its public tests need no
original files or Java. `MC_ENABLE_STORAGE_REFERENCE_TESTS=ON` adds
`reference.level`, `reference.region_write`, `reference.chunk_storage` and
`reference.world_storage` with
both supplied JARs, JDK17+ and `MC_REFERENCE_TESTED_COMMIT`. Use fresh owned or
copied test worlds. At `9ad690d83592f127b9bb39d7233e8eef06fd0029`, all 71
selected tests pass without skips on native MSVC/GCC/Clang, with Linux
ASan/UBSan and independent copied-world/platform audits. See
[storage evidence](reference/storage.md#exact-source-storage-checkpoint)
and the [world ownership contract](world/storage.md). These library operations
do not provide a playable world loader or complete gameplay save scheduling.

The lifecycle comparison is selected by `MC_ENABLE_LIFECYCLE_REFERENCE_TESTS=ON`.
It uses both supplied JAR paths and `Java_JAVA_EXECUTABLE` /
`Java_JAVAC_EXECUTABLE` from the same JDK17+ installation. Ten serial
`reference.lifecycle_*` tests create fresh preparation/observed/control worlds,
validate original captures and run the native comparison. The normal build's
`world.lifecycle_callbacks_replay` needs no Java or original archives. See
[lifecycle evidence](reference/lifecycle.md) for profiles, limits and source
qualification. The accepted source, full matrix, independent audits and hosted
results are recorded in the [lifecycle ledger](reference/lifecycle.md#exact-source-lifecycle-checkpoint).
Dedicated `unload101` alone permits 600 seconds in the controller and 660 seconds
in the exporter; other profile and data bounds remain unchanged. These tests do
not start a gameplay server.

The separate controller mirror regression needs JDK17+ with `jdk.jdi` and no
original archives or assets:

```sh
python tests/tools/cache/check_controller_mirrors.py --work-dir build/controller-mirrors
```

It fails for missing tools, inconclusive collection or unstable recorded IDs and
runs separately from CTest. See [testing](testing.md) for its disposal, capacity
and duplicate-label controls.

Runtime code is organized as `src/<system>/<subsystem>/`, with matching public
headers under `include/mc/` and tests under `tests/`. See [architecture](architecture.md)
for current and planned modules. `.clang-format` sets four spaces, a 100-column
limit, attached braces, and blank lines between definitions. CI uses clang-format18:

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
and [binary setup](reference/binaries.md). The [native title screen](client/startup.md)
consumes a subset of those resources. Keep originals, launcher files,
credentials, recordings, logs and worlds in
ignored local directories. Copy a world before any writable reference test.

Current compiler/platform results and remaining graphical access limits are in
[the resume note](status.md). Client controls, gameplay save support and hosting
instructions will accompany actual implementations.
