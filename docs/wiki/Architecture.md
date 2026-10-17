# Systems and subsystems

Project-owned runtime code uses C17. Implementations live under
`src/<system>/<subsystem>/`; public headers mirror those paths under `include/mc/`,
and tests mirror them under `tests/`. Create folders with actual implementations
rather than empty placeholders.

| Current module | Responsibility | Runtime dependencies |
| --- | --- | --- |
| `core/java` | Defined integer/cast/Random behavior | C runtime |
| `core/text` | Modified UTF-8 with explicit UTF-16 units | C runtime |
| `storage/nbt` | Owned raw tag trees, original IDs 0..10, bounded read/write | Shared core |
| `storage/compression` | Owned gzip/zlib memory output, explicit limits and checksums | Static zlib |

Neither shared library initializes a window, graphics, audio, networking, assets,
or Java. The raw NBT parser and compression module remain separate interfaces;
callers explicitly compose them. No file write or world schema is implicit.

The planned game systems include ordered world/simulation updates, platform
files/sockets/clock/input services, OpenGL 3.3 rendering, audio, original protocol
networking, client sessions, and a dedicated server. Those game systems are not
implemented yet. The server must remain independent of client display/audio
initialization, and shared simulation must retain evidenced original side
differences.

Strict project diagnostics and floating-point rules are configured in CMake.
`.clang-format` specifies four spaces, 100 columns, and Linux-style braces; CI
checks project C files with clang-format18. Reference tooling is kept under
`scripts/reference/<subsystem>/` and `tests/reference/<subsystem>/`. Ignored
original research inputs are excluded from formatting and publication.

Read the versioned
[architecture](https://github.com/frankischilling/minecraft-c/blob/main/docs/architecture.md)
and [roadmap](https://github.com/frankischilling/minecraft-c/blob/main/docs/roadmap.md)
for current module details and remaining dependencies. The
[parity matrix](https://github.com/frankischilling/minecraft-c/blob/main/docs/parity.md)
records scenario-specific evidence; a shared-library test is not gameplay parity.
