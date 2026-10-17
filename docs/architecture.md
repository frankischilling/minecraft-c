# Runtime boundaries

Project-owned runtime code uses C17. `mc_core` contains numerical compatibility
and allocation-free modified UTF-8 text helpers. `mc_storage` depends on it and
contains the raw NBT reader/writer and gzip/zlib memory compression. Neither library initializes display,
graphics, audio, networking, assets or Java. Tests and research helpers are
separate executables/scripts; none is a hidden game engine.

The [float lookup/floor module](core/math.md) uses a caller-owned 65536-float
table initialized once with the measured system double-sine backend. Later
lookups share it as const data; the Java cast/compare floor needs no table.
There is no hidden global initializer or Java dependency in this module.

`mc_world` depends on `mc_core` and implements the original seeded
Perlin/octave [noise module](world/noise.md) and the separate
[2D simplex/octave family](world/simplex.md). The noise primitives own no allocations
or platform services. Caller-supplied layer arrays keep construction and shared-Random order
explicit. Bulk sampling preserves the original height-one path and Y-cell cache.

The [climate and biome module](world/biome.md) composes independently seeded
simplex channels with original climate processing and float-table lookup.
Caller-owned managers retain mutable climate/biome buffers under an explicit
allocation budget. External temperature/direct biome outputs stay separate
from those retained fields; calls stage growth before publishing changes.

The [normal terrain module](world/terrain.md) composes all original seeded
Perlin groups into density, then interpolates base blocks. Immutable constructor
state is separate from borrowed workspace/output spans and processed climate.
Its fixed normal shape retains original flat layouts and buffer suffix behavior.

The [normal surface pass](world/surface.md) reuses immutable terrain layers,
accepts current top/filler byte pairs and advances a separate caller-owned
Random. Field sampling and material application remain usable independently;
composed calls validate all borrowed storage before any mutation.

The [normal cave pass](world/caves.md) uses a separate caller-owned Random,
immutable math table and existing block prefix. It owns the fixed neighbor
schedule, ordered local path Randoms, geometry and selective material changes.
Checked preflight precedes mutation; successful calls allocate nothing.
Private room/path helpers serve both normal runtime and focused reference adapters.

The [chunk voxel-data module](world/chunk.md) borrows the block buffer and embeds
independent packed metadata/light arrays plus column heights. It provides bounded
nibble access and local height/direct-skylight rebuilding without allocation.
World cache queries, lighting queues, dirty state and chunk lifecycle remain
separate responsibilities.

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

## System and subsystem folders

Runtime implementations live at `src/<system>/<subsystem>/`; public headers mirror
that path under `include/mc/`, and tests mirror it under `tests/`. This keeps a
subsystem's implementation and interface easy to locate as the project grows.
Create new folders with actual implementations rather than empty placeholders.

```text
include/mc/core/java/java.h
src/core/java/java.c
tests/core/java/test_java.c
include/mc/core/math/math.h
src/core/math/math.c
tests/core/math/test_math.c
tests/core/math/math_probe.c
tests/reference/math/TrigMathObservation.java
scripts/reference/math/compare_math.py
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
include/mc/world/noise/noise.h
src/world/noise/noise.c
tests/world/noise/test_noise.c
tests/world/noise/noise_probe.c
tests/reference/noise/NoiseProbe.java
scripts/reference/noise/compare_noise.py
include/mc/world/noise/simplex.h
src/world/noise/simplex.c
tests/world/noise/test_simplex.c
tests/world/noise/simplex_probe.c
tests/reference/simplex/SimplexProbe.java
scripts/reference/simplex/compare_simplex.py
include/mc/world/biome/biome.h
src/world/biome/biome.c
tests/world/biome/test_biome.c
tests/world/biome/biome_probe.c
tests/reference/biome/BiomeProbe.java
scripts/reference/biome/compare_biome.py
include/mc/world/terrain/terrain.h
src/world/terrain/terrain.c
tests/world/terrain/test_terrain.c
tests/world/terrain/terrain_probe.c
tests/reference/density/DensityProbe.java
scripts/reference/density/compare_density.py
include/mc/world/terrain/surface.h
src/world/terrain/surface.c
tests/world/terrain/test_surface.c
tests/world/terrain/surface_probe.c
tests/reference/surface/SurfaceProbe.java
scripts/reference/surface/compare_surface.py
include/mc/world/terrain/caves.h
src/world/terrain/caves.c
src/world/terrain/caves_internal.h
tests/world/terrain/test_caves.c
tests/world/terrain/caves_probe.c
tests/reference/caves/CavesObservation.java
scripts/reference/caves/compare_caves.py
include/mc/world/chunk/data.h
include/mc/world/chunk/nibbles.h
src/world/chunk/data.c
src/world/chunk/nibbles.c
tests/world/chunk/test_data.c
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

Formatting follows the repository `.clang-format` and is
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

Add interfaces as reference-backed game behavior is implemented.
CMake build options apply strict diagnostics
and floating-point rules to project code; future dependencies need their own
licensing and build review. SDL3 is a candidate, and its current APIs must be
verified before use. Storage uses pinned static zlib; its
[license and build configuration](dependencies.md) remain separate from project code.

Use checked allocation, bounded buffers, and explicit ownership/cleanup paths.
Begin with reproducible simulation order. Parallel research/build tasks do not
authorize concurrent simulation updates without equivalence evidence.
