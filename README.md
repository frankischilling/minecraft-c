# minecraft-c

A C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows and Linux.
The target is a graphical single-player/multiplayer client and a dedicated
headless server, reproducing the original behavior with locally selected
usernames and no account authentication.

There is no playable C client or server yet. Current code provides numerical
and text compatibility, bounded NBT/gzip/zlib, [seeded terrain noise](docs/world/noise.md), tests, reference
tools, and a local resource importer. The [parity matrix](docs/parity.md) records
verified scenarios and outstanding work.

## Build

Use a C17 compiler, CMake 3.24+, Ninja, and Python 3.10+. On Windows, run from a
Visual Studio developer shell. Initial configuration downloads pinned zlib.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

See [build setup](docs/building.md) for platform details, offline dependencies,
formatting, resource import, and optional reference checks.

## Documentation

- [Architecture and subsystem folders](docs/architecture.md)
- [Testing and original-reference evidence](docs/testing.md)
- [Feature inventory](docs/inventory.md) and [roadmap](docs/roadmap.md)
- [Current development status](docs/status.md)
- [Project wiki](https://github.com/frankischilling/minecraft-c/wiki)

New code and original project documentation use [MIT](LICENSE); dependencies
retain their [own licenses](docs/dependencies.md). Minecraft binaries, assets,
decompiled source, launcher data, and worlds are excluded from this repository
and releases. Supply original resources locally when using the importer.
