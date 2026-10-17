# minecraft-c

A C17 implementation of Minecraft Beta 1.7.3 for native x86-64 Windows and Linux.
The intended deliverables are a graphical single-player and multiplayer client
and a dedicated headless server. Account authentication is excluded; players will
select local usernames.

There is no playable C client or server yet. The current implementation contains
a shared numerical compatibility module, public tests, local reference tools,
and an asset importer. [The parity matrix](docs/parity.md) records verified
scenarios and outstanding work. The full target remains a 1:1 rewrite.

Original Minecraft binaries, assets, decompiled source, launcher data, and worlds
are local research inputs and are excluded from this repository and releases.
The new code and original project documentation use the [MIT license](LICENSE).
That license does not cover Minecraft binaries or assets.

## Build and test

Use a C17 compiler, CMake 3.24 or newer, and Python 3.10 or newer for tool tests.
The initial target is x86-64. No display, OpenGL, SDL, assets, or Java are needed
to build the shared core or run the public tests.

Linux, with GCC or Clang and Ninja installed:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Native Windows, from a Visual Studio developer shell with its C tools installed:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

For a Visual Studio multi-configuration generator, use `cmake -S . -B build -A x64`,
then add `--config Debug` to the build and `-C Debug` to CTest. The existing code
has been exercised locally with native MSVC and with GCC/Clang on Linux under WSL;
see [testing](docs/testing.md) for scope and exact environments. Interactive client
playability and a C server runtime remain unimplemented.

Enable `-DMC_ENABLE_SANITIZERS=ON` for GCC/Clang address and undefined behavior
checks. Enable `-DMC_ENABLE_JAVA_REFERENCE_TESTS=ON` to add the numerical
differential against an installed JDK 8 or newer. Requested Java tests fail
configuration when the JDK is missing. They do not use Minecraft files or prove
gameplay parity. Normal C executables have no JVM dependency.

Code is grouped by system and subsystem, with matching headers and tests:
`src/core/java/`, `include/mc/core/java/`, and `tests/core/java/` are the current
runtime example. Tools use `scripts/resources/` and `scripts/reference/<subsystem>/`.
The repository's `.clang-format` specifies four-space indentation, a 100-column
limit, and Linux-style braces. Format or check all project C files with:

```sh
python scripts/development/format_code.py
python scripts/development/format_code.py --check
```

CI uses clang-format 18. Select it explicitly with `--clang-format clang-format-18`
when several versions are installed.

## Local resources and reference research

Supply your own verified client JAR and external resource directory:

```powershell
python scripts/resources/import_assets.py --jar b1.7.3.jar --resources "$env:APPDATA/.minecraft/resources" --output .local/assets
python scripts/reference/binaries/inspect_jars.py --client b1.7.3.jar --server b1.7.3_server.jar --output .local/reference/jars.json
```

On Linux, pass explicit Linux paths; Windows launcher paths are not portable.
The importer requires a fresh output directory and lists missing inputs. It
copies only selected resource paths, with bounds and path checks. See
[resource evidence and limits](docs/reference/resources.md). There is no native
asset consumer yet. Imported assets are intended to serve normal gameplay
without retaining the JAR, launcher, Java, or account services.

Read [binary research](docs/reference/binaries.md), the [feature inventory](docs/inventory.md),
[architecture](docs/architecture.md), and [roadmap](docs/roadmap.md). The
[resume note](docs/status.md) records the current integration state. Client controls,
save compatibility, and hosting instructions will be added with their implementations;
there is currently no rewrite executable to play or host.
