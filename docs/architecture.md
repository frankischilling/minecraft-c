# Runtime boundaries

Project-owned runtime code uses C17. `mc_core` contains numerical compatibility
and allocation-free modified UTF-8 text helpers. `mc_storage` depends on it and
contains raw NBT, gzip/zlib memory compression and borrowed region record views.
Neither library initializes display,
graphics, audio, networking, assets or Java. Tests and research helpers are
separate executables/scripts; none is a hidden game engine.

The public header is the boundary for Java arithmetic and explicitly seeded
`Random`. Callers own each generator. Invalid divisors and bounds return failure
without changing outputs or consuming randomness; game-specific exception behavior
belongs to the caller. See [evidence](reference/java-semantics.md) for operation
coverage and the remaining semantics audit.

The [NBT boundary](storage/nbt.md) owns retained decoded trees and budgets their
allocations. It accepts bytes and produces bytes without compression, streams
or world schemas. Strings preserve UTF-16 code units, and parser failures leave
caller result outputs unchanged. The separate compression module owns its
decoded bytes and validates whole member checksums. Callers compose these
interfaces; neither implicitly selects a world schema or opens a file.
The separate `mc_region_inspect` executable opens files only for reading and
composes region, compression and NBT modules for selected local records.

## System and subsystem folders

Runtime implementations live at `src/<system>/<subsystem>/`; public headers mirror
that path under `include/mc/`, and tests mirror it under `tests/`. This keeps a
subsystem's implementation and interface easy to locate as the project grows.
Create new folders with actual implementations rather than empty placeholders.

```text
include/mc/core/java/java.h
src/core/java/java.c
tests/core/java/test_java.c
include/mc/core/text/mutf8.h
src/core/text/mutf8.c
tests/core/text/test_mutf8.c
include/mc/storage/nbt/nbt.h
src/storage/nbt/nbt.c
tests/storage/nbt/test_nbt.c
tests/storage/nbt/nbt_probe.c
include/mc/storage/compression/compression.h
src/storage/compression/compression.c
tests/storage/compression/test_compression.c
tests/storage/compression/compression_probe.c
include/mc/storage/region/region.h
src/storage/region/region.c
tests/storage/region/test_region.c
tests/storage/region/region_probe.c
src/tools/storage/region_inspect.c
tests/tools/region/test_inspect.py
tests/reference/java/JavaSemanticsProbe.java
tests/reference/nbt/NbtProbe.java
tests/tools/resources/test_import_assets.py
scripts/development/format_code.py
scripts/resources/import_assets.py
scripts/reference/binaries/inspect_jars.py
scripts/reference/java/compare_java.py
scripts/reference/nbt/compare_nbt.py
scripts/reference/server/run_smoke.py
```

The source layout and `.clang-format` are tracked by
[#5](https://github.com/frankischilling/minecraft-c/issues/5). Formatting is
repeatable through `scripts/development/format_code.py` and checked in CI with
clang-format 18. The formatter scans project C headers/sources only, keeping
ignored decompiled research outside its input set.

## Planned game systems

As implementation proceeds, the intended boundaries are:

| Module | Responsibility | Dependency rule |
| --- | --- | --- |
| Core | Defined Java numerical/text/serialization behavior | Usable by headless tools and server |
| World and simulation | Chunks, blocks, entities, ordered tick updates, generation | No rendering or audio initialization; preserve evidenced side distinctions |
| Persistence | NBT, region files, world/player/entity state | Preserve unimplemented fields before writable compatibility paths |
| Networking | Original packet encoding, stream framing, connection states | Bounded parsing and explicit buffering; original protocol |
| Platform | Clock, files, sockets, window/input/audio device services | Native Linux/Windows implementations with explicit ownership |
| Renderer and audio | OpenGL 3.3 core presentation, resource loading, audio events | Linked by graphical client only |
| Client | Menus, input sampling, single-player and multiplayer sessions | Render frequency must not change simulation state |
| Dedicated server | Headless configuration, administration, save and shutdown | No display, OpenGL context, or client audio dependency |

Beyond the core and raw NBT boundary, these are design directions rather than
implemented game systems. Add interfaces when a reference-backed behavior needs
them. CMake build options apply strict diagnostics
and floating-point rules to project code; future dependencies need their own
licensing and build review. SDL3 is a candidate, and its current APIs must be
verified before use. Storage uses pinned static zlib; its
[license and build configuration](dependencies.md) remain separate from project code.

Use checked allocation, bounded buffers, and explicit ownership/cleanup paths.
Begin with reproducible simulation order. Parallel research/build tasks do not
authorize concurrent simulation updates without equivalence evidence.
