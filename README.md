# minecraft-c

A C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows and Linux.
The target is a graphical single-player/multiplayer client and a dedicated
headless server, reproducing the original behavior with locally selected
usernames and no account authentication.

There is no playable C client or server yet. Shared libraries cover numerical
and text compatibility, NBT/compression, and terrain-generation components.
Tests, reference tools and a local resource importer support development.
The [parity matrix](docs/parity.md) records verified scenarios and outstanding work.
The default client runs the constructed source App and its
[menus](docs/client/menu-runtime.md), including real save-list selection and
Cancel. Existing-save loading reaches World and Player/input publication;
renderer attachment, picking, gameplay input and
[focused mouse look](docs/client/renderer-frames.md) run. Pause entry and ordered
World ticks reach concrete Mob construction. World drawing now completes fog,
sky, camera, ChunkRenderer culling and dirty-chunk selection. Real cache/list
compilation now emits standard Block geometry through the actual
[RenderBlocks path](docs/client/render-blocks.md). Terrain submission, specialized
Block shapes and dynamic frames remain unfinished. A separate
[generated-world inspection](docs/client/world-renderer.md) remains available.

## Build

Use a C17 compiler, CMake 3.24+, Ninja, and Python 3.10+. On Windows, run from a
Visual Studio developer shell. Initial configuration downloads pinned zlib,
SDL3, libpng, FreeType, libogg, libvorbis, OpenAL Soft, curl and Expat. Client audio
needs a C++20 compiler for OpenAL Soft; project-owned runtime code remains C17.
Linux builds need the platform
development packages listed in [build setup](docs/building.md).

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
